# Relatório AP2: Convolução 2D paralela com OpenMP

## 1. Problema resolvido e código-base

### 1.1 Problema

O problema é o mesmo da AP1: um filtro de convolução 2D (stencil) 3×3 sobre uma matriz N×N de `float`.
A máscara é um blur gaussiano:

```
            | 1 2 1 |
K = 1/16 ×  | 2 4 2 |
            | 1 2 1 |
```

Cada ponto interno da saída é a soma ponderada dos 9 vizinhos da entrada (9 multiplicações e 8 somas,
17 FLOP). Só os pontos internos são calculados (`1 ≤ r, c ≤ N-2`). Maiores detalhes no relatório da AP1.

### 1.2 Versão escolhida da AP1

O enunciado pede para paralelizar **a versão com melhor desempenho da AP1**. Na AP1, a melhor versão foi
a **SIMD pragma + aligned** (`vectorize/`, binário `simd_aligned`): laço por linha, `#pragma omp simd
aligned(input, output: 32)` no laço interno, `__restrict__` nos ponteiros e `aligned_alloc(32, ...)`.

| N | SIMD pragma + aligned | OpenCV (1 thread) |
|---|---:|---:|
| 1024 | 0,248 ms | 0,337 ms |
| 2048 | 1,473 ms | 1,884 ms |
| 4096 | 6,758 ms | 8,744 ms |

### 1.3 Finalidade do código

O código foi criado para este trabalho. A finalidade é ter um kernel pequeno, com acesso regular à
memória, para isolar o efeito de cada otimização. Na AP2, o objetivo é medir:

1. o ganho de desempenho com várias threads (speedup e eficiência);
2. o custo em potência e o ganho (ou a perda) em energia;
3. o efeito da arquitetura híbrida (P-cores, Hyper-Threading e E-cores);
4. o efeito do posicionamento das threads e da política de escalonamento do OpenMP.

O mesmo tipo de kernel aparece em processamento de imagens (blur, detecção de bordas) e em simulações
com diferenças finitas (equação do calor, Jacobi).

**Dados de entrada.** Tamanhos N = 1024, 2048 e 4096. A matriz é gerada no próprio programa com valores
aleatórios entre 0 e 255 e semente fixa (`srand(42)`). Assim, todas as execuções usam a mesma entrada.

## 2. Estrutura do projeto

Código-fonte: <https://github.com/lucaomsilva/conv2D>

### 2.1 Módulo `parallel/`

A AP2 adiciona o módulo `parallel/`, com a mesma estrutura dos módulos da AP1. A versão base continua no
módulo `vectorize/` (`simd_aligned`).

| Módulo | Papel na AP2 | Binário | Linha de comando |
|---|---|---|---|
| `vectorize/` | Versão base (AP1, 1 thread) | `simd_aligned` | `<N>` |
| `library/` | Comparação com OpenCV (1 thread) | `library_o3` | `<N>` |
| `parallel/` | Versão paralela (OpenMP + SIMD) | `parallel_omp` | `<N> <threads>` |

```
parallel/
├── Makefile                  # build, test, run, asm, format, clean
├── src/
│   ├── conv/conv2d.c         # kernel: omp parallel for (linhas) + omp simd (colunas)
│   ├── conv/conv2d.h         # protótipo e máscaras 3×3
│   └── main.c                # driver: alocação, dados, calibração, medição, JSON
├── tests/test_conv.c         # corretude: N × threads × escalonamentos
└── scripts/
    ├── parallel_omp.sh       # experimentos (reference, main, places, schedules)
    └── profile_metrics.py    # perf + OMP_PLACES/OMP_PROC_BIND/OMP_SCHEDULE, CSV
```

O binário imprime uma linha JSON com os campos da AP1 e três campos novos:

```
JSON_METRICS:{"kernel_time_s": ..., "reps": ..., "total_time_s": ..., "energy_j": ...,
              "threads": 16, "schedule": "static", "chunk": 0}
```

### 2.2 Fluxo de dados

```
parallel/scripts/parallel_omp.sh    ativa o perfil "Desempenho" (tuned), laço sobre experimentos × N × threads
        │
        ▼
profile_metrics.py                  10 execuções: espera ≤ 60 °C → perf stat + OMP_* → binário
        │                           lê JSON_METRICS e os contadores, divide por reps
        ▼
docs/data/parallel/*.csv            um CSV por configuração (216 configurações)
        │
        ▼
scripts/compile_data.py             summary.csv (mediana), speedup em relação a 1 thread, eficiência
        │
        ▼
scripts/analysis/                   ap2_compare.py → resume.md; ap2_plots.py → figures/
```

## 3. Equipamento e ferramentas

| Item | Valor |
|---|---|
| CPU | Intel Core i5-12500H (Alder Lake, híbrida) |
| Núcleos | 4 P-cores com HT (CPUs 0–7; pares 0/1, 2/3, 4/5, 6/7) + 8 E-cores sem HT (CPUs 8–15) = 16 CPUs lógicas |
| Frequência máxima | 4,5 GHz (P-core), 3,3 GHz (E-core) |
| Cache L1d | 48 KiB por P-core (32 KiB por E-core) |
| Cache L2 | 1,25 MiB por P-core (2 MiB compartilhado por grupo de 4 E-cores) |
| Cache L3 (LLC) | 18 MiB compartilhado |
| Memória | 15 GiB |
| Instruções vetoriais | AVX2, FMA. **Sem AVX-512** |
| Sistema | Fedora 43, kernel Linux 7.0.12 |
| Perfil de energia | `tuned` `throughput-performance`: governor `performance`, EPP `performance`, ACPI `platform_profile` = `performance` |
| Compilador e OpenMP | gcc 15.2.1, runtime libgomp (`-fopenmp`) |
| Biblioteca | OpenCV 4.11.0 (framework TBB, sem Intel IPP) |
| Contadores | `perf` 7.2.9 (`perf stat`) |
| Energia | Intel RAPL pela interface powercap (`/sys/class/powercap/intel-rapl:0/energy_uj`, domínio *package*) |

**Tamanho dos dados × cache L3.** Entrada + saída ocupam 8 MiB (N = 1024), 32 MiB (N = 2048) e 128 MiB
(N = 4096). Só N = 1024 cabe na L3 de 18 MiB. Esse fato explica boa parte dos resultados (seção 8.3).

## 4. Metodologia de medição

### 4.1 Ferramentas e eventos

| Ferramenta | Uso |
|---|---|
| `perf stat` | `cycles`, `instructions` e misses de cache, somados nos P-cores (`cpu_core`) e nos E-cores (`cpu_atom`). |
| RAPL (powercap) | Energia do pacote da CPU (todos os núcleos, L3, controlador de memória), lida pelo próprio binário. |
| `clock_gettime` | Tempo do laço medido. |

A CPU híbrida tem dois tipos de unidade de contadores. Os eventos da AP1 não servem nos E-cores:
`L1-dcache-load-misses` não existe em `cpu_atom`, e `LLC-load-misses` conta quase nada lá (135 mil contra
191 milhões de `longest_lat_cache.miss`, N = 2048, 16 threads). Por isso, a AP2 usa eventos com a mesma
definição nos dois tipos de núcleo:

| Métrica | P-core (`cpu_core`) | E-core (`cpu_atom`) |
|---|---|---|
| Misses L1 | `mem_load_retired.l1_miss` | `mem_load_uops_retired.l1_miss` (mesmo código: evento 0xd1, umask 8) |
| Misses L2 | `l2_request.miss` | `l2_request.miss` |
| Misses L3 | `longest_lat_cache.miss` | `longest_lat_cache.miss` |

Consequência: **os misses da AP2 não podem ser comparados com os da AP1.** A comparação usa a execução
com 1 thread da própria AP2.

### 4.2 Quantidade de threads e posicionamento

A máquina tem **16 CPUs lógicas**
(4 P-cores × 2 HT + 8 E-cores × 1). Mais que 16 threads só faria o sistema operacional revezar threads na
mesma CPU.

Na AP1, o processo ficava preso na CPU 2 com `taskset`. Na AP2, isso prenderia todas as threads em uma
CPU. O posicionamento passa a ser do OpenMP, com duas variáveis de ambiente:

| Variável | O que faz | Valor usado |
|---|---|---|
| `OMP_PLACES` | Define a lista de *places* (lugares) onde as threads podem rodar. Cada place é um conjunto de CPUs lógicas. | Um place por CPU lógica, na ordem da tabela abaixo. Ex.: `{2},{4},{6},{0},…` |
| `OMP_PROC_BIND` | Define como as threads são distribuídas nos places e se elas ficam presas a eles. | `close`: a thread 0 fica no 1º place, a thread 1 no 2º, e assim por diante. A thread não migra. Na ordem `none`, o valor é `false`: sem binding. |

Assim, com T threads, o programa usa os T primeiros places da lista. A ordem da lista define quais CPUs
entram primeiro. A distribuição foi conferida com `OMP_DISPLAY_AFFINITY=true`, que faz o runtime do OpenMP
imprimir a CPU de cada thread no início da região paralela.

| Ordem | Lista de CPUs (`OMP_PLACES`) | Ideia |
|---|---|---|
| **`p-ht-e`** (principal) | 2, 4, 6, 0 → 3, 5, 7, 1 → 8 … 15 | Um thread por P-core, depois os irmãos HT, depois os E-cores |
| `ht-p-e` | 2, 3, 4, 5, 6, 7, 0, 1 → 8 … 15 | Preenche o HT antes de usar outro P-core |
| `p-e-ht` | 2, 4, 6, 0 → 8 … 15 → 3, 5, 7, 1 | Usa os E-cores antes do HT |
| `none` | sem binding | O Linux escolhe e pode migrar as threads |

A lista começa na CPU 2, como na AP1 (a CPU 0 trata mais interrupções do sistema). Assim, a execução com
1 thread roda no mesmo núcleo da AP1. Com `p-ht-e`, as threads ocupam três regiões: **1–4** só P-cores
físicos, **5–8** P-cores com HT, **9–16** P-cores com HT + E-cores.

### 4.3 Experimentos

| Experimento | Configuração | Threads | Configurações |
|---|---|---|---:|
| `reference` | `simd_aligned` e OpenCV, 1 thread na CPU 2, mesma sessão | 1 | 6 |
| `main` | `p-ht-e`, `static` | 1–16 | 48 |
| `places` | `ht-p-e`, `p-e-ht`, `none`, `static` | 1, 2, 4, 6, …, 16 | 81 |
| `schedules` | `dynamic,1`, `dynamic,16`, `guided`, `p-ht-e` | 1, 2, 4, 6, …, 16 | 81 |
| **Total** | 3 tamanhos × 10 execuções cada | | **216** |

O `simd_aligned` e o OpenCV foram medidos de novo na mesma sessão. Assim, a comparação com a AP1 usa as
mesmas condições (perfil de energia, temperatura, versão do sistema).

**Escalonamentos.** O kernel usa `schedule(runtime)`: a política vem da variável `OMP_SCHEDULE`, sem
recompilar.

| Política | Como divide as 1022 linhas internas |
|---|---|
| `static` | Em T blocos iguais e contíguos, definidos no início. Custo quase zero. |
| `dynamic,c` | Fila de blocos de c linhas. Cada thread pega um bloco novo quando termina o anterior. |
| `guided` | Como `dynamic`, mas os blocos começam grandes e diminuem. |

Os experimentos `main` e `places` usam `static`. É a política de uso comum: ela é o padrão do GCC
(libgomp) quando o `schedule` não é informado. Ela também é a escolha natural para este kernel, porque
todas as linhas têm o mesmo custo. O experimento `schedules` compara as outras políticas com ela.

### 4.4 Como cada execução é medida

O método é o mesmo da AP1:

```
[ início | alocação | dados | 1 chamada sem medição | calibração | KERNEL × reps | fim ]
                                                                 ▲               ▲
                                      perf liga, lê RAPL, relógio┘               └ relógio, lê RAPL, perf desliga
```

1. **Chamada sem medição.** Ela paga os page faults da saída e cria o pool de threads do OpenMP. Medido:
   com N = 1024 e 16 threads, a 1ª chamada leva 1,0 ms e as seguintes 0,19 ms; com N = 4096 e 1 thread,
   19,6 ms contra 6,8 ms.
2. **Calibração.** `reps = 1 s ÷ tempo de 1 chamada + 1`.
3. **Laço medido de ≈ 1 s.** O RAPL atualiza a cada ≈ 1 ms, então uma chamada curta não pode ser medida
   sozinha.
4. **Contadores só no laço.** O `perf stat -D -1 --control fifo:...` começa desligado; o binário liga e
   desliga os contadores por uma FIFO.
5. **Valores por chamada.** Energia e contadores são divididos por `reps`.
6. **Mesma temperatura inicial.** O script espera o sensor `x86_pkg_temp` ficar ≤ 60 °C antes de cada
   execução (medido: ≈ 1 s de espera, mesmo com 16 threads).
7. **Perfil de energia fixo.** O script ativa o perfil `throughput-performance` do `tuned` (o mesmo modo
   "Desempenho" usado na AP1) e restaura o perfil anterior no fim, também com Ctrl+C.
8. **Repetições.** 10 execuções (processos separados) por configuração. As tabelas usam a **mediana**,
   como na AP1. A mediana não é afetada por uma execução fora do padrão. Exemplo: N = 1024, `p-ht-e`,
   15 threads, uma execução levou 0,297 ms e as outras ≈ 0,077 ms; a média ficaria 28 % acima da mediana.
   Nas outras 215 configurações, média e mediana diferem pouco (mediana das diferenças: 0,4 %).

### 4.5 Fórmulas (por chamada do kernel)

- MFLOPS = (N − 2)² × 17 ÷ (10⁶ × tempo)
- Potência (W) = energia do laço ÷ tempo do laço
- Energia (J) = energia do laço ÷ reps
- Eficiência energética = MFLOPS ÷ W (MFLOPS/W = milhões de FLOP por joule)
- **Speedup** S(p) = T(1) ÷ T(p), com a mesma ordem e o mesmo escalonamento
- **Eficiência paralela** E(p) = S(p) ÷ p
- **Speedup AP1** = T(`simd_aligned`, mesma sessão) ÷ T(p)

### 4.6 Qualidade dos dados

| Verificação | Resultado |
|---|---|
| Configurações | 216, cada uma com 10 execuções válidas (2160 execuções) |
| Energia e contadores | Nenhuma leitura com valor 0 |
| Temperatura inicial | 44–60 °C; nenhuma execução começou acima de 60 °C |
| Potência (mediana) | 32,6–85,7 W |
| Frequência com 1 thread (cycles ÷ tempo) | 4,37–4,38 GHz |
| Variação do tempo (desvio ÷ média) | mediana 2,1 %; 90 % das configurações ≤ 5,3 % |
| `simd_aligned` × OpenMP com 1 thread | 0,228 × 0,226 ms (N = 1024): o custo do OpenMP com 1 thread é desprezível |

Anomalias:
- N = 1024, `p-ht-e`, 15 threads: a 1ª execução levou 0,297 ms (as outras 9: ≈ 0,077 ms). Ela começou a
  59 °C, contra 45 °C nas outras. A mediana não é afetada.
- N = 1024, `ht-p-e`, 1 thread: mediana de 0,272 ms, contra 0,226 ms do `p-ht-e`. As duas usam a mesma
  CPU 2, então a diferença é ruído de medição. Ela não é usada nas conclusões.

## 5. Pragmas e alterações em relação à AP1

| Versão | Módulo | Flags | Alteração |
|---|---|---|---|
| Base (AP1) | `vectorize` | `-O3 -mavx2 -mfma -fopenmp-simd -DMODE_ALIGNED` | `omp simd aligned` + `__restrict__` + `aligned_alloc(32)` |
| Paralela (AP2) | `parallel` | `-O3 -mavx2 -mfma -fopenmp` | + `omp parallel for schedule(runtime) firstprivate(k)` no laço `r` |

```diff
 void conv2d(const float *__restrict__ input, float *__restrict__ output, int N,
             const float kernel[3][3]) {
+    float k[3][3];                       // cópia local dos coeficientes
+    for (int i = 0; i < 3; i++)
+        for (int j = 0; j < 3; j++)
+            k[i][j] = kernel[i][j];
+
+#pragma omp parallel for schedule(runtime) firstprivate(k)
     for (int r = 1; r < N - 1; r++) {
 #pragma omp simd aligned(input, output: 32)
         for (int c = 1; c < N - 1; c++) {
             float sum = 0.0f;
             for (int kr = -1; kr <= 1; kr++)
                 for (int kc = -1; kc <= 1; kc++)
-                    sum += input[(r + kr) * N + (c + kc)] * kernel[kr + 1][kc + 1];
+                    sum += input[(r + kr) * N + (c + kc)] * k[kr + 1][kc + 1];
             output[r * N + c] = sum;
         }
     }
 }
```

**Pragmas usados:**

| Pragma / cláusula | Efeito |
|---|---|
| `#pragma omp parallel for` | Cria a equipe de threads e divide as linhas `r` entre elas. Cada linha da saída é escrita por uma só thread, então não há condição de corrida. |
| `schedule(runtime)` | A política de divisão vem de `OMP_SCHEDULE` (`static`, `dynamic,c`, `guided`). |
| `firstprivate(k)` | Cada thread recebe sua cópia dos 9 coeficientes. |
| `#pragma omp simd aligned(input, output: 32)` | Mantém a vetorização AVX2 da AP1 no laço `c` de cada thread. |

**Por que a cópia local dos coeficientes.** Com só o `parallel for`, a versão com 1 thread ficou 35 % mais
lenta que o `simd_aligned` (0,30 × 0,23 ms, N = 1024). O OpenMP move o corpo do laço para uma função
separada. Nessa função, o GCC perde a informação do `__restrict__` e passa a reler os 9 coeficientes da
máscara a cada iteração. Com a cópia local (`firstprivate(k)`), o tempo volta a 0,24 ms. A troca de
`schedule(runtime)` por `schedule(static)` não muda o tempo.

**Outras alterações:**
- `main.c` recebe o número de threads (`omp_set_num_threads(T)`) e escreve `threads`, `schedule` e `chunk`
  no JSON.
- `profile_metrics.py` não usa mais `taskset`; ele define `OMP_PLACES`, `OMP_PROC_BIND` e `OMP_SCHEDULE`.

## 6. Corretude

Os testes (`make parallel-test`) comparam a versão paralela com uma referência escalar (tolerância
`1e-4`, por causa do FMA) em todas as combinações de:

- N = 3 (menor matriz válida), 64, **65 (ímpar)** e 1024;
- 1, 2, 7 e 16 threads. As combinações incluem divisões com resto (ex.: N = 64 tem 62 linhas, que não
  se dividem igualmente entre 7 ou 16 threads) e mais threads que linhas (N = 3 tem 1 linha);
- escalonamentos `static`, `dynamic` e `guided`.

A saída começa preenchida com um valor sentinela (−1). O teste também verifica que a **borda não é
escrita**. Como na AP1, os índices de leitura ficam em `0 ≤ r+kr, c+kc ≤ N-1`. A paralelização só divide
as linhas `1 … N-2` entre as threads; ela não muda os índices.

## 7. Resultados

Valores por chamada do kernel, mediana de 10 execuções.

### 7.1 Melhor versão da AP1 × OpenCV × OpenMP

| N | Versão | Tempo (ms) | Speedup | Potência (W) | Energia (mJ) | MFLOPS/W |
|---|---|---:|---:|---:|---:|---:|
| 1024 | `simd_aligned` (1 thread) | 0,228 | 1,00× | 34,4 | 7,82 | 2272 |
| | OpenCV `filter2D` (1 thread) | 0,339 | 0,67× | 34,3 | 11,62 | 1528 |
| | OpenMP mais rápido (T = 12, `p-ht-e`, `dynamic,16`) | **0,065** | **3,51×** | 82,1 | 5,34 | 3327 |
| | OpenMP melhor MFLOPS/W (T = 4, `static`) | 0,073–0,078 | 2,9–3,1× | 63–66 | **4,87–4,92** | **3607–3648** |
| 2048 | `simd_aligned` (1 thread) | 1,414 | 1,00× | 34,2 | 48,57 | 1465 |
| | OpenCV `filter2D` (1 thread) | 1,850 | 0,76× | 35,4 | 65,62 | 1085 |
| | OpenMP mais rápido e melhor MFLOPS/W (T = 13, `p-ht-e`, `static`) | **0,530** | **2,67×** | 69,0 | **36,54** | **1948** |
| 4096 | `simd_aligned` (1 thread) | 6,680 | 1,00× | 33,8 | 225,66 | 1263 |
| | OpenCV `filter2D` (1 thread) | 8,691 | 0,77× | 34,7 | 302,30 | 943 |
| | OpenMP mais rápido (T = 8, `p-ht-e`, `guided`) | **3,286** | **2,03×** | 56,1 | 183,98 | 1549 |
| | OpenMP melhor MFLOPS/W (T = 6, `ht-p-e`, `static`) | 3,670 | 1,82× | 48,7 | **178,80** | **1594** |

Em N = 1024, o melhor MFLOPS/W aparece em `p-e-ht` (3648) e em `p-ht-e` (3607) com 4 threads. Com 4 threads,
as duas ordens usam as mesmas CPUs (2, 4, 6, 0), então a diferença é ruído.

O OpenCV `filter2D` não usa mais de um núcleo neste filtro: com `cv::setNumThreads(16)`, o tempo de CPU é
igual ao tempo real (1,857 × 1,864 s), e o tempo por chamada não muda (N = 4096: 9,7 / 9,5 / 9,6 ms com
1 / 4 / 16 threads). Por isso, ele foi medido só com 1 thread.

### 7.2 Escalabilidade (`p-ht-e`, `static`)

Cada linha é uma quantidade de threads (T). Para cada N, há três colunas: o tempo por chamada,
**S** = speedup = T(1) ÷ T(p) e **E** = eficiência paralela = S ÷ p. S = 2,0 é duas vezes mais rápido que
1 thread. E = 1,0 é o ganho ideal; E = 0,5 é metade do ganho ideal.

| T | N = 1024 (ms) | S | E | N = 2048 (ms) | S | E | N = 4096 (ms) | S | E |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0,226 | 1,00 | 1,00 | 1,409 | 1,00 | 1,00 | 6,751 | 1,00 | 1,00 |
| 2 | 0,127 | 1,77 | 0,89 | 0,967 | 1,46 | 0,73 | 4,794 | 1,41 | 0,70 |
| 3 | 0,092 | 2,46 | 0,82 | 0,786 | 1,79 | 0,60 | 4,085 | 1,65 | 0,55 |
| 4 | 0,078 | 2,89 | 0,72 | 0,706 | 1,99 | 0,50 | 3,572 | 1,89 | 0,47 |
| 5 | 0,098 | 2,31 | 0,46 | 0,738 | 1,91 | 0,38 | 3,777 | 1,79 | 0,36 |
| 6 | 0,086 | 2,62 | 0,44 | 0,678 | 2,08 | 0,35 | 3,634 | 1,86 | 0,31 |
| 7 | 0,082 | 2,74 | 0,39 | 0,651 | 2,16 | 0,31 | 3,542 | 1,91 | 0,27 |
| 8 | 0,077 | 2,92 | 0,36 | 0,653 | 2,16 | 0,27 | **3,318** | **2,03** | 0,25 |
| 9 | 0,092 | 2,47 | 0,27 | 0,607 | 2,32 | 0,26 | 3,455 | 1,95 | 0,22 |
| 10 | 0,085 | 2,66 | 0,27 | 0,587 | 2,40 | 0,24 | 3,543 | 1,91 | 0,19 |
| 11 | 0,082 | 2,77 | 0,25 | 0,557 | 2,53 | 0,23 | 3,531 | 1,91 | 0,17 |
| 12 | 0,085 | 2,66 | 0,22 | 0,568 | 2,48 | 0,21 | 3,787 | 1,78 | 0,15 |
| 13 | 0,082 | 2,76 | 0,21 | **0,530** | **2,66** | 0,20 | 3,775 | 1,79 | 0,14 |
| 14 | 0,079 | 2,84 | 0,20 | 0,539 | 2,61 | 0,19 | 3,765 | 1,79 | 0,13 |
| 15 | 0,078 | 2,89 | 0,19 | 0,583 | 2,42 | 0,16 | 3,829 | 1,76 | 0,12 |
| 16 | **0,077** | **2,95** | 0,18 | 0,614 | 2,29 | 0,14 | 3,843 | 1,76 | 0,11 |

### 7.3 Potência, energia e MFLOPS/W (`p-ht-e`, `static`)

| T | N = 1024: W | mJ | MFLOPS/W | N = 2048: W | mJ | MFLOPS/W | N = 4096: W | mJ | MFLOPS/W |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 34,7 | 7,84 | 2265 | 34,3 | 48,37 | 1471 | 33,6 | 226,9 | 1256 |
| 2 | 49,7 | 6,40 | 2775 | 46,0 | 44,40 | 1603 | 44,9 | 215,0 | 1325 |
| 4 | 63,4 | **4,92** | **3607** | 54,1 | 38,30 | 1858 | 52,0 | 185,8 | 1534 |
| 8 | 68,0 | 5,27 | 3371 | 59,4 | 38,72 | 1838 | 55,9 | **185,1** | **1540** |
| 12 | 68,4 | 5,82 | 3052 | 67,1 | 37,96 | 1875 | 57,3 | 217,4 | 1311 |
| 13 | 69,8 | 5,70 | 3116 | 69,0 | **36,54** | **1948** | 58,7 | 222,1 | 1283 |
| 16 | 69,8 | 5,34 | 3325 | 64,5 | 39,69 | 1793 | 58,3 | 225,2 | 1265 |

### 7.4 Ordem dos núcleos (`static`, tempo em ms)

Em negrito, o menor tempo de cada linha.

**N = 1024**

| T | `p-ht-e` | `ht-p-e` | `p-e-ht` | `none` |
|---:|---:|---:|---:|---:|
| 1 | **0,226** | 0,272 | 0,229 | 0,231 |
| 2 | **0,127** | 0,223 | 0,137 | 0,134 |
| 4 | 0,078 | 0,119 | **0,073** | 0,091 |
| 6 | **0,086** | 0,095 | 0,125 | 0,132 |
| 8 | 0,077 | **0,075** | 0,116 | 0,112 |
| 10 | 0,085 | **0,084** | 0,105 | 0,099 |
| 12 | **0,085** | 0,086 | 0,096 | 0,087 |
| 14 | 0,079 | 0,081 | 0,082 | **0,078** |
| 16 | 0,077 | 0,078 | 0,080 | **0,075** |

**N = 2048**

| T | `p-ht-e` | `ht-p-e` | `p-e-ht` | `none` |
|---:|---:|---:|---:|---:|
| 1 | **1,409** | 1,422 | 1,422 | 1,419 |
| 2 | 0,967 | 1,137 | **0,932** | 0,940 |
| 4 | **0,706** | 0,794 | 0,718 | 0,747 |
| 6 | 0,678 | 0,711 | 0,680 | **0,662** |
| 8 | **0,653** | 0,655 | 0,799 | 0,674 |
| 10 | **0,587** | 0,596 | 0,733 | 0,714 |
| 12 | **0,568** | 0,578 | 0,711 | 0,653 |
| 14 | **0,539** | 0,543 | 0,617 | 0,596 |
| 16 | 0,614 | 0,623 | 0,608 | **0,587** |

**N = 4096**

| T | `p-ht-e` | `ht-p-e` | `p-e-ht` | `none` |
|---:|---:|---:|---:|---:|
| 1 | 6,751 | **6,677** | 6,800 | 6,956 |
| 2 | 4,794 | 5,283 | **4,777** | 4,883 |
| 4 | 3,572 | 4,203 | **3,549** | 3,696 |
| 6 | **3,634** | 3,670 | 3,796 | 3,810 |
| 8 | 3,318 | **3,294** | 4,115 | 3,801 |
| 10 | 3,543 | **3,523** | 4,006 | 3,858 |
| 12 | **3,787** | 3,810 | 3,847 | 3,813 |
| 14 | 3,765 | 3,779 | 3,856 | **3,753** |
| 16 | 3,843 | 3,887 | 3,863 | **3,773** |

### 7.5 Escalonamento (`p-ht-e`, tempo em ms)

Em negrito, o menor tempo de cada linha.

**N = 1024**

| T | `static` | `dynamic,1` | `dynamic,16` | `guided` |
|---:|---:|---:|---:|---:|
| 1 | **0,226** | 0,258 | 0,232 | 0,261 |
| 2 | **0,127** | 0,168 | 0,136 | 0,134 |
| 4 | 0,078 | 0,105 | 0,074 | **0,073** |
| 6 | 0,086 | 0,094 | 0,079 | **0,076** |
| 8 | 0,077 | 0,088 | 0,075 | **0,070** |
| 10 | 0,085 | 0,079 | 0,066 | **0,065** |
| 12 | 0,085 | 0,079 | **0,065** | 0,072 |
| 14 | 0,079 | 0,075 | **0,066** | 0,068 |
| 16 | 0,077 | 0,084 | **0,068** | 0,070 |

**N = 2048**

| T | `static` | `dynamic,1` | `dynamic,16` | `guided` |
|---:|---:|---:|---:|---:|
| 1 | 1,409 | 1,450 | **1,406** | 1,459 |
| 2 | 0,967 | 1,123 | **0,886** | 0,920 |
| 4 | 0,706 | 0,914 | **0,684** | 0,685 |
| 6 | 0,678 | 0,889 | **0,665** | 0,674 |
| 8 | 0,653 | 0,875 | 0,668 | **0,638** |
| 10 | 0,587 | 0,885 | 0,590 | **0,586** |
| 12 | **0,568** | 0,908 | 0,579 | 0,579 |
| 14 | **0,539** | 0,916 | 0,597 | 0,585 |
| 16 | **0,614** | 0,997 | 0,665 | 0,636 |

**N = 4096**

| T | `static` | `dynamic,1` | `dynamic,16` | `guided` |
|---:|---:|---:|---:|---:|
| 1 | **6,751** | 6,833 | 6,765 | 6,790 |
| 2 | 4,794 | 4,951 | **4,625** | 4,725 |
| 4 | 3,572 | 4,016 | **3,422** | 3,504 |
| 6 | 3,634 | 4,024 | **3,380** | 3,451 |
| 8 | 3,318 | 4,039 | 3,421 | **3,286** |
| 10 | 3,543 | 3,890 | 3,512 | **3,504** |
| 12 | 3,787 | 3,937 | 3,616 | **3,586** |
| 14 | 3,765 | 3,816 | 3,605 | **3,597** |
| 16 | 3,843 | 4,084 | 3,845 | **3,793** |

### 7.6 Misses de cache por chamada (`p-ht-e`, `static`)

| T | N = 1024: L1 | L2 | L3 | N = 2048: L1 | L2 | L3 | N = 4096: L1 | L2 | L3 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 5,8 mil | 132 mil | 460 | 65 mil | 929 mil | 275 mil | 0,99 M | 3,71 M | 1,72 M |
| 4 | 14 mil | 130 mil | 75 | 155 mil | 753 mil | 245 mil | 1,79 M | 2,46 M | 1,36 M |
| 8 | 11 mil | 134 mil | 106 | 238 mil | 634 mil | 200 mil | 1,24 M | 2,23 M | 1,25 M |
| 9 | 80 mil | 143 mil | 268 | 551 mil | 1,39 M | 246 mil | 8,67 M | 5,58 M | 3,12 M |
| 16 | 131 mil | 146 mil | 233 | 1,14 M | 1,22 M | 415 mil | 13,2 M | 5,59 M | 3,52 M |

### 7.7 Gráficos

Escalabilidade (`p-ht-e`, `static`, uma linha por N):

![Tempo](figures/threads_time.png)
![Speedup](figures/threads_speedup.png)
![Eficiência paralela](figures/threads_efficiency.png)
![MFLOPS](figures/threads_mflops.png)
![Potência](figures/threads_power.png)
![Energia](figures/threads_energy.png)
![MFLOPS/W](figures/threads_mflops_per_watt.png)
![Misses L1](figures/threads_l1.png)
![Misses L2](figures/threads_l2.png)
![Misses L3](figures/threads_l3.png)

Ordem dos núcleos e escalonamento:

![Tempo por ordem](figures/places_time.png)
![MFLOPS/W por ordem](figures/places_mflops_per_watt.png)
![Tempo por escalonamento](figures/schedule_time.png)
![MFLOPS/W por escalonamento](figures/schedule_mflops_per_watt.png)

## 8. Análise comparativa

### 8.1 Melhor versão da AP1 × OpenMP

- O OpenMP é **3,51× mais rápido** que o `simd_aligned` em N = 1024, **2,67×** em N = 2048 e **2,03×** em
  N = 4096.
- O ganho é bem menor que o número de threads. Com 16 CPUs lógicas, o speedup ideal seria 16×. O motivo
  principal é a memória (seção 8.3).
- A versão paralela com 1 thread tem o mesmo tempo do `simd_aligned` (0,226 × 0,228 ms). O custo do OpenMP
  sozinho é desprezível, desde que os coeficientes sejam copiados para variáveis locais (seção 5).
- Comparado ao OpenCV com 1 thread, o melhor OpenMP é **5,2× mais rápido** em N = 1024, 3,5× em N = 2048 e
  2,6× em N = 4096. Neste filtro, o OpenCV não ganha nada com mais threads.

### 8.2 Escalabilidade e arquitetura híbrida

O gráfico de speedup tem três regiões, que seguem a ordem `p-ht-e`:

1. **P-cores físicos (T = 1–4).** É a região de melhor eficiência paralela. Com N = 1024, o speedup chega
   a 2,89× em T = 4 (E = 0,72). Com N maior, a eficiência cai mais cedo (N = 4096: E = 0,47 em T = 4).
2. **HT (T = 5–8).** Em T = 5, o tempo **piora** (N = 1024: 0,078 → 0,098 ms). A 5ª thread divide um P-core
   com a 1ª. Duas threads no mesmo núcleo dividem as unidades AVX2 e as caches L1/L2, então cada uma fica
   mais lenta. Com `static`, todas as threads recebem a mesma quantidade de linhas, e as outras esperam a
   mais lenta. O tempo só volta ao nível de T = 4 quando todos os P-cores têm 2 threads (T = 8).
3. **E-cores (T = 9–16).** Em T = 9, a mesma queda se repete (N = 1024: 0,077 → 0,092 ms): a 9ª thread roda
   em um E-core, mais lento (3,3 GHz, outra microarquitetura). Com N = 1024 e 2048, os E-cores ajudam
   pouco a pouco (2048: melhor tempo em T = 13). Com N = 4096, eles **pioram** o tempo (3,32 ms em T = 8 →
   3,84 ms em T = 16), porque a memória já está saturada.

### 8.3 Memória e cache

- **N = 1024 (8 MiB, cabe na L3).** É o caso com melhor escalabilidade. Os misses de L3 ficam em
  centenas por chamada: os dados ficam no chip.
- **N = 2048 e 4096 (32 e 128 MiB, não cabem na L3).** Cada chamada traz a matriz da DRAM. O limite passa a
  ser a **largura de banda da memória**, que é compartilhada por todos os núcleos. Com N = 4096, cada
  chamada lê 64 MiB e escreve 64 MiB. Em 3,32 ms (T = 8), isso dá cerca de **40 GB/s** úteis (≈ 60 GB/s
  contando a leitura extra da linha antes da escrita). Com 1 thread, são 20 GB/s. Mais threads não aumentam
  esse valor, então o speedup para em ≈ 2×.
- Por isso, o speedup máximo **cai quando N cresce**: 2,95× (`static`) / 3,51× (melhor) em N = 1024,
  2,66× em N = 2048 e 2,03× em N = 4096.
- **Misses nos E-cores.** Em T = 9, os misses de L3 de N = 4096 sobem de 1,25 M para 3,12 M por chamada. A
  matriz tem só ≈ 2 M linhas de cache (64 MiB lidos + 64 MiB escritos). O evento `longest_lat_cache.miss`
  dos E-cores parece contar também pedidos de prefetch. Por isso, os misses só são comparados dentro da
  região de P-cores (T ≤ 8). Os misses de L1 também sobem a partir de T = 9 (1,24 M → 8,67 M em N = 4096):
  além da L1 menor do E-core (32 KiB), o evento do E-core conta micro-operações de carga, e o E-core
  provavelmente divide cada carga AVX2 de 256 bits em duas de 128 bits.

### 8.4 Ordem dos núcleos e escalonamento

**Ordem dos núcleos.**
- Com T = 1 e T = 16, as ordens com binding usam as mesmas CPUs, então os tempos são iguais (diferença
  de ruído).
- **`ht-p-e` (HT primeiro) é a pior com poucas threads.** N = 1024, T = 2: 0,223 ms, contra 0,127 ms com
  `p-ht-e`. Duas threads no mesmo P-core quase não somam desempenho.
- **`p-e-ht` e `none` são piores de 6 a 10 threads**, principalmente nos tamanhos grandes (N = 4096,
  T = 8: 4,12 ms em `p-e-ht` e 3,80 ms em `none`, contra 3,32 ms em `p-ht-e`). O E-core entra cedo e, com
  `static`, atrasa todas as threads.
- **`none` (sem binding)** às vezes empata, às vezes perde até 50 % (N = 1024, T = 6: 0,132 × 0,086 ms). O
  Linux escolhe CPUs diferentes a cada execução, e o resultado fica imprevisível.
- **Conclusão:** `p-ht-e` é a melhor ordem ou empata com a melhor na maior parte dos casos. Ela confirma a
  escolha inicial.

**Escalonamento.**
- **`dynamic,16` e `guided` reduzem o desequilíbrio do `static`.** O experimento `schedules` só mediu
  números pares, então não tem T = 5 e T = 9. Mesmo assim, nos pontos onde o `static` ainda sofre com o
  núcleo mais lento, as duas políticas ganham: N = 1024, T = 6: 0,076–0,079 × 0,086 ms; T = 10: 0,065–0,066 ×
  0,085 ms. Quando uma thread está em um núcleo mais lento (HT ou E-core), ela simplesmente pega menos
  blocos. Em N = 1024, isso leva ao melhor
  tempo geral: 0,065 ms (T = 10–12), contra 0,077 ms do melhor `static`.
- **`dynamic,1` é a pior política** em quase todos os casos (N = 2048, T = 8: 0,875 × 0,653 ms). Pedir uma
  linha por vez à fila custa mais que calcular a linha (≈ 0,2 µs por linha em N = 1024).
- **Em N = 2048, o `static` vence a partir de T = 12** (0,568 ms, contra 0,579 ms; melhor geral 0,530 ms em T = 13).
- O custo do OpenMP também aparece com **1 thread**: `dynamic,1` leva 0,258 ms, contra 0,226 ms do `static`
  (N = 1024), por causa das 1022 retiradas de blocos da fila.

### 8.5 Watts × joules

- **A potência sobe com as threads:** de 34 W (1 thread) para 56–82 W, conforme N e a política. Mais
  núcleos ativos consomem mais.
- **A energia por chamada cai**, porque o tempo cai mais do que a potência sobe:

| N | Energia com 1 thread | Menor energia (OpenMP) | Redução |
|---|---:|---:|---:|
| 1024 | 7,84 mJ | 4,87 mJ (T = 4) | −38 % |
| 2048 | 48,37 mJ | 36,54 mJ (T = 13) | −24 % |
| 4096 | 226,9 mJ | 178,8 mJ (T = 6, `ht-p-e`) | −21 % |

- **O ponto mais rápido não é sempre o mais econômico.** Em N = 1024, o mais rápido (T = 12, `dynamic,16`,
  82 W) gasta 5,34 mJ; o mais econômico (T = 4, 63–66 W) gasta 4,87–4,92 mJ com 12–20 % a mais de tempo.
- **Quando a memória satura, mais threads só gastam energia.** Em N = 4096, de T = 8 a T = 16 o tempo
  piora e a potência sobe; a energia volta quase ao valor de 1 thread (225 × 227 mJ).
- **Usar menos núcleos físicos pode ser melhor.** Em N = 4096, o melhor MFLOPS/W é `ht-p-e` com 6 threads
  (3 P-cores com HT, 48,7 W). Ele é 12 % mais lento que o mais rápido, mas usa 3 % menos energia, porque a
  memória limita o tempo de qualquer forma.
- Uma parte da potência é fixa (L3, controlador de memória, sistema). Terminar mais rápido economiza essa
  parte, e isso explica a queda da energia mesmo com a potência maior.

## 9. Limitações

- **RAPL mede o pacote inteiro** (todos os núcleos, L3, controlador de memória). Não é possível separar a
  energia de cada núcleo nem a da DRAM.
- **Misses em CPU híbrida.** Os eventos dos E-cores têm o mesmo código, mas não a mesma microarquitetura
  (seção 8.3). Os misses com E-cores devem ser lidos com cuidado.
- **Misses da AP2 × AP1.** Os eventos mudaram (seção 4.1), então os misses não são comparáveis entre as
  duas atividades.
- **Frequência variável.** O turbo ficou ligado. Com muitas threads, os limites de potência e de
  temperatura reduzem a frequência; isso faz parte do resultado, mas não foi medido por thread.
- **OpenCV com 1 thread.** O `filter2D` não paraleliza este filtro nesta instalação (TBB, sem IPP).

## 10. Conclusões

1. **O OpenMP acelerou a melhor versão da AP1 em 2,0–3,5×**, com uma linha de pragma e uma cópia local
   dos coeficientes. Sem essa cópia, o GCC perde o `__restrict__` e a versão paralela fica 35 % mais lenta
   com 1 thread.
2. **A escalabilidade é limitada pela memória.** Só N = 1024 cabe na L3. Para N ≥ 2048, a largura de banda
   da DRAM limita o speedup a ≈ 2–2,7×, mesmo com 16 threads.
3. **A arquitetura híbrida cria desequilíbrio de carga.** Com `static`, a primeira thread em HT (T = 5) e a
   primeira em E-core (T = 9) deixam o programa mais lento. `dynamic,16` e `guided` reduzem esse efeito e
   dão o melhor tempo em N = 1024 e 4096; `dynamic,1` é caro demais.
4. **A ordem das threads importa.** Um thread por P-core primeiro (`p-ht-e`) é a melhor ordem ou empata
   com a melhor. Sem binding, o resultado fica imprevisível.
5. **Paralelizar reduziu a energia em 21–38 %**, mesmo com até o dobro da potência. O ponto mais econômico
   usa menos threads que o mais rápido (N = 1024: 4 threads; N = 4096: 6 threads em 3 núcleos).
6. **O OpenCV não paraleliza este filtro** e fica 2,6–5,2× atrás do melhor OpenMP.

## 11. Como reproduzir

```bash
git clone https://github.com/lucaomsilva/conv2D.git && cd conv2D
make parallel-build vectorize-build-aligned library-build
make parallel-test                 # corretude: N × threads × escalonamentos
make parallel-run                  # 216 configurações × 10 execuções (≈ 2 h 25; precisa de sudo)
make install/analysis-tools        # .venv com matplotlib (uma vez)
make analysis/ap2                  # docs/report/ap2/resume.md e figures/
```

Variáveis para uma execução menor: `EXPERIMENTS`, `SIZES`, `THREADS`, `CMP_THREADS`, `RUNS`. Exemplo:

```bash
EXPERIMENTS=main SIZES=1024 THREADS="1 16" RUNS=2 make parallel-run
```
