# Notas de metodologia de medição (porquês)

Notas para escrever a seção de metodologia do relatório.
Implementado nos quatro módulos (`src/main.c` / `main.cpp` e `scripts/profile_metrics.py` de cada um).

## 1. Problema do método antigo

O `profile_metrics.py` lia o RAPL **antes e depois do processo inteiro**:

```
Python lê RAPL ─┐                                              ┌─ Python lê RAPL
                [ inicia | malloc | rand() N² | KERNEL | free | sai ]
                                              └ 2,9 ms ┘
                └───────────── energia medida (processo inteiro) ──────────────┘
```

- A energia incluía: início do processo, `malloc`, preenchimento com `rand()` e saída.
- O tempo (`kernel_time_s`) era só o kernel (`clock_gettime` no C).
- Potência = energia ÷ tempo dividia a energia de um intervalo longo pelo tempo de um intervalo curto.

Exemplo real (`baseline_o3`, N=1024, row, dados antigos em `docs/data_bkp/`):

| | Valor |
|---|---|
| Energia (processo inteiro) | 0,560 J |
| Tempo do kernel | 0,00288 s |
| Potência calculada | 194 W (impossível: o i5-12500H fica em ~45 W) |

A energia em joules também estava errada. Ela era quase toda da inicialização, que é igual em todas
as versões. Por isso, em N=1024, todas as versões davam ~0,5 J, mesmo com tempos de 0,4 ms a 2,9 ms.

Os contadores do `perf stat` tinham o mesmo problema: contavam o processo inteiro. Exemplo:
o OpenCV mostrava ~2,7×10⁸ L1 misses em todos os N (início da biblioteca, não o `filter2D`).

## 2. Método novo

```
[ inicia | malloc | rand() | chamada sem medição | calibração | KERNEL × reps | free | sai ]
                                                               ▲             ▲
                                          perf liga, lê RAPL,  ┘             └ para relógio,
                                          inicia relógio                      lê RAPL, perf desliga
                                                               └─── ≥ ~1 s ──┘
```

Tempo, energia e contadores cobrem **o mesmo intervalo**: só o laço do kernel.
Isso está na função `func_energy()` em `baseline/src/main.c`.

### 2.1 Por que medir dentro do C e não no Python

O Python está fora do processo. Ele só pode ler o RAPL antes de o processo começar e depois de
ele terminar. O C sabe exatamente quando o kernel começa e termina. O Python agora só chama o
binário, divide pelos `reps` e grava o CSV.

### 2.2 Por que repetir o kernel `reps` vezes

O RAPL não é um medidor contínuo:

- O contador `energy_uj` atualiza a cada ~1 ms. Entre duas atualizações o valor não muda.
- Ele mede o pacote inteiro (todos os núcleos, uncore, outros processos).

Exemplo: `simd_aligned`, N=1024 (kernel ~0,4 ms, ~30 W):

| | 1 chamada | ~2500 chamadas (~1 s) |
|---|---|---|
| Energia real | ~12 mJ | ~30 J |
| O RAPL pode mostrar | 0 mJ ou ~30 mJ | 30 J ± 0,03 J |
| Erro | ≥ 100% | ~0,1% |

O erro de uma atualização no início e uma no fim fica pequeno quando o intervalo é longo.
O ruído de outros processos também se dilui.

Escolha de `reps` (calibração): `reps = TARGET_TIME_S / tempo_de_1_chamada + 1`, com
`TARGET_TIME_S = 1.0`. Kernel rápido → muitas repetições. Kernel lento (ex.: -O0 col N=4096, ~1 s)
→ ~2 repetições. Não é preciso escolher manualmente.

A comparação entre versões não muda: o CSV guarda valores **por chamada** (energia ÷ reps,
contadores ÷ reps). A potência é a média real do laço: energia do laço ÷ tempo do laço.

### 2.3 Por que uma chamada sem medição dentro do C (além do warm-up do Python)

Cada warm-up do Python é **um processo novo**. O sistema libera a memória quando o processo termina,
e o próximo processo recebe páginas novas. O warm-up do Python não aquece:

- **Page faults:** o `calloc` de 4 MB (N=1024) usa `mmap`. As páginas só são mapeadas na primeira
  escrita. A primeira chamada do kernel escreve em `output` e gera ~1000 page faults (4 MB ÷ 4 KB).
- **TLB e caches** com os dados deste processo.

A chamada sem medição paga esse custo antes da medição. Ela também protege a calibração. Se a
primeira chamada for lenta por causa dos page faults, `reps` fica pequeno e o laço não chega a 1 s.

### 2.4 Por que ligar/desligar o perf pelo C (FIFO de controle)

`perf stat -D -1 --control fifo:ctl,ack` inicia com os contadores desligados. O binário escreve
`enable`/`disable` na FIFO e espera o `ack` do perf. Assim os contadores só contam o laço.
Os caminhos vêm das variáveis `PERF_CTL_FIFO` e `PERF_ACK_FIFO`. Sem elas, o binário roda normal.

Resultado (N=1024, -O3, row): L1 misses caíram de ~97 mil (processo inteiro) para ~6–12 mil por chamada.

### 2.5 Por que fixar o processo em um P-core (`taskset -c 2`)

O i5-12500H é híbrido: P-cores (CPUs 0–7, `cpu_core`) e E-cores (CPUs 8–15, `cpu_atom`).
Sem fixar, um processo que passa um tempo curto em um E-core recebe uma contagem **estimada**
(escalada) do `cpu_atom`. Exemplo medido:

```
7,367,025,373  cpu_atom/instructions/u   (0.75%)   ← estimativa a partir de 0,75% do tempo
11,684,695,845 cpu_core/instructions/u   (99.25%)
```

A soma deu ~58 M instruções por chamada, em vez de 35,5 M. Isso também deixava a CPI errada.

| Sem fixar (8 execuções, N=1024) | Fixado na CPU 2 |
|---|---|
| 35,5 M a 58,0 M instruções/chamada | 35.524.765–35.524.766 em todas |
| CPI 0,24–0,26 | CPI 0,233–0,244 |

A CPU 2 foi escolhida em vez da CPU 0 porque a CPU 0 trata mais interrupções do sistema.

### 2.6 O warm-up do Python ainda vale a pena?

Teste com `--warmup 0` (8 execuções fixadas na CPU 2). A primeira execução não é outlier:

| Config | 1ª execução | Faixa das outras 7 |
|---|---|---|
| -O3 row N=1024 | 1,955 ms | 1,864–1,947 ms |
| -O3 col N=4096 | 346,3 ms | 343,5–354,3 ms |

Com a chamada sem medição e o laço de ~1 s, o warm-up do Python quase não muda o resultado.
O que ele ainda pode ajudar: frequência da CPU e cache de disco do binário, mas a calibração
e o laço de 1 s já levam a CPU ao turbo. Ele também não estabiliza a temperatura (isso leva dezenas
de segundos). Custo: 5 × ~1,2 s por configuração.

**Decisão:** warm-up do Python removido. A flag `--warmup` não existe mais nos profilers e nos scripts dos quatro módulos.

### 2.7 Por que esperar a CPU esfriar antes de cada execução

As execuções rodam em sequência (~1 s de carga cada). O notebook esquenta, e a frequência turbo
e a potência podem cair nas últimas execuções. Isso deixaria a comparação injusta.

Antes de cada execução medida, o `profile_metrics.py` lê o sensor `x86_pkg_temp`
(`/sys/class/thermal/thermal_zone*/temp`) e espera até a temperatura do pacote ficar ≤ 60 °C
(máximo de 60 s). Temperatura em repouso medida: ~56 °C; o limite é ~repouso + 5 °C.
A temperatura de início vai para a coluna `start_temp_c` do CSV, para mostrar no relatório que
todas as execuções começaram na mesma condição.

## 3. Efeitos para relatar

- **Estado estável (cache quente):** os tempos ficam menores que os antigos.
  Tamanho dos dois buffers (`input` + `output`) comparado com o L3 (18 MB):

  | N | Buffers | Cabe no L3? | -O3 row: antigo → novo |
  |---|---|---|---|
  | 1024 | 2 × 4 MB = 8 MB | Sim | 2,88 ms → 1,94 ms |
  | 2048 | 2 × 16 MB = 32 MB | Não | 11,47 ms → (coleta final) |
  | 4096 | 2 × 64 MB = 128 MB | Não | 45,68 ms → 30–32 ms |

  - N=1024: as repetições encontram os dados no L3. A diferença row × col tende a ficar menor
    que no método antigo, porque o acesso por coluna é servido pelo L3 e não pela DRAM.
  - N=2048 e 4096: os dados não cabem no L3, então o ganho não vem do cache. Ele vem da chamada sem
    medição: no método antigo, a chamada medida era a primeira escrita no `output` (64 MB em N=4096
    ≈ 16 mil page faults de 4 KB dentro do tempo medido).
- **RAPL precisa de root:** `energy_uj` só é legível pelo root neste sistema. Sem `sudo` o binário
  imprime `energy_j: -1` e o CSV grava energia 0. Os scripts de execução já usam `sudo`.
- **Contadores em modo usuário sem root:** sem `sudo`, o perf conta só modo usuário (`/u`).
  Com `sudo`, conta também o kernel. Use sempre `sudo` para os dados finais.

## 4. Fórmulas (por chamada do kernel)

- Tempo = tempo do laço ÷ reps
- MFLOPS = (N−2)² × 17 ÷ (10⁶ × tempo)
- Energia (J) = energia do laço ÷ reps
- Potência (W) = energia do laço ÷ tempo do laço
- MFLOPS/W = MFLOPS ÷ potência
- Contadores (misses, cycles, instructions) = contagem do laço ÷ reps; CPI = cycles ÷ instructions

## 5. OpenCV: só 1 thread em 1 P-core

`library_o3 <N>` chama `cv::setNumThreads(1)` e o profiler roda com `taskset -c 2` (um P-core).
É a mesma condição dos kernels próprios (1 thread, mesmo núcleo).

Por que não medir com mais threads (teste feito antes da coleta final, sem `sudo`):

- 1 thread (CPU 2) × padrão do OpenCV (8 threads, CPUs 0–7): tempo por chamada quase igual.
  N=1024: 0,364 ms × 0,374 ms; N=4096: 9,56 ms × 9,38 ms.
- `perf stat -e task-clock` com 8 threads: task-clock 1540 ms ≈ tempo total 1546 ms → **só ~1 CPU usada**.
- `opencv_version -v`: framework paralelo TBB, mas **sem Intel IPP** e sem HAL próprio (Fedora não inclui IPP).
  Sem IPP, o `filter2D` usa o `FilterEngine` do OpenCV, que não divide a imagem entre threads.
- Conclusão: no OpenCV 4.11, `cv::filter2D` com kernel 3×3 `CV_32F` **não é paralelizado**.
  O ganho do OpenCV vem de SIMD (AVX2; ≈3,6 M instruções por chamada em N=1024 ≈ 3,5 instruções por pixel), não de threads.

## 6. Vetorização: efeito do `__restrict__` × `aligned` (análise do assembly)

`simd_aligned` muda duas coisas em relação a `simd_unaligned`: `__restrict__` nos ponteiros e a cláusula
`aligned(input, output: 32)`. Para separar os efeitos, foi compilada uma variante só com `__restrict__`
(sem `aligned`), fora do projeto, e comparado o laço interno (`gcc -O3 -mavx2 -mfma -fopenmp-simd -S`).

Laço interno (8 floats por iteração):

| Variante | Leituras do kernel no laço | Instruções no laço |
|---|---|---|
| autovec (sem pragma) | 0 (pesos em registradores) | 9 `vfmadd` + 1 `vmovups` |
| pragma (`MODE_UNALIGNED`) | **8 `vbroadcastss`** (relê os pesos a cada iteração) | 9 `vfmadd` + 8 `vbroadcastss` + 1 `vmovups` |
| só `__restrict__` | 0 | 9 `vfmadd` + 1 `vmovups` |
| `__restrict__` + `aligned` (`MODE_ALIGNED`) | 0 | 9 `vfmadd` + 1 `vmovups` (igual ao "só restrict") |

- Sem `__restrict__`, o compilador não pode provar que `output` não sobrepõe `kernel`. Com `#pragma omp simd`
  ele vetoriza sem teste de sobreposição, mas precisa reler os 9 pesos a cada iteração.
- Sem pragma (autovec), o GCC gera um teste de sobreposição em tempo de execução e, na versão vetorizada,
  mantém os pesos em registradores. Por isso o autovec fica igual ao `restrict`.
- A cláusula `aligned` não muda o laço: os acessos começam em `c = 1` (e usam `c-1`, `c+1`), então os
  endereços não são múltiplos de 32 bytes. O laço continua com `vmovups` e FMA com operando de memória.

Tempo por chamada (cache quente, CPU 2, média de muitas repetições):

| Variante | N=1024 | N=4096 |
|---|---|---|
| autovec | 0,236 ms | 7,19 ms |
| pragma (unaligned) | 0,330 ms | 9,02 ms |
| só `__restrict__` | 0,243 ms | 8,03 ms |
| `__restrict__` + `aligned` | 0,254 ms | 7,82 ms |

Conclusões:

1. O ganho de `simd_aligned` sobre `simd_unaligned` vem do `__restrict__`, não do alinhamento.
2. O alinhamento de 32 bytes não tem efeito mensurável neste stencil.
3. O `#pragma omp simd` sozinho deixou o código **mais lento** que a autovetorização.
4. O "3,5×" dos dados antigos era efeito do método antigo: `MODE_ALIGNED` faz `memset` no `output` antes
   da medição (páginas já mapeadas), e os outros modos usam `calloc` (page faults dentro da chamada medida).
