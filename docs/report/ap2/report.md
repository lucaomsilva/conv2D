# Relatório AP2: Convolução 2D paralela com OpenMP

## 1. Problema resolvido e código-base

### 1.1 Problema

TODO: resumir o stencil 3×3 (blur gaussiano, 17 FLOP por ponto). Referenciar o relatório da AP1 (`../ap1/report.md`, seção 1).

### 1.2 Versão escolhida da AP1

TODO: justificar `simd_aligned` como a melhor versão da AP1 (tabela de tempo da AP1). Explicar por que a blocagem fica de fora (não foi significativamente melhor).

### 1.3 Finalidade do código

TODO: finalidade do kernel e dos tamanhos testados (N = 1024, 2048, 4096). Dados de entrada gerados no programa (`srand(42)`, valores 0–255).

## 2. Estrutura do projeto

### 2.1 Módulo `parallel/`

TODO: árvore do módulo, binário `parallel_omp <N> <T>`, alvos do `Makefile`. Versão base = `vectorize/` (`simd_aligned`).

### 2.2 Fluxo de dados

TODO: `parallel_omp.sh` → `profile_metrics.py` → `docs/data/parallel/` → `compile_data.py` → `scripts/analysis/` → `docs/report/ap2/`.

## 3. Equipamento e ferramentas

TODO: tabela da AP1 + topologia híbrida (CPUs 0–7 = 4 P-cores × 2 HT; CPUs 8–15 = 8 E-cores sem HT), frequências máximas (4,5 / 3,3 GHz), runtime OpenMP (libgomp, GCC 15.2.1).

## 4. Metodologia de medição

### 4.1 Ferramentas e eventos

TODO: RAPL (pacote), `perf stat` com eventos de mesma semântica em `cpu_core` e `cpu_atom`.

### 4.2 Quantidade de threads e posicionamento

TODO: lista de threads, `OMP_PLACES`, `OMP_PROC_BIND`, ordem de ocupação dos núcleos. Por que o `taskset` da AP1 saiu.

### 4.3 Como cada execução é medida

TODO: mesmo método da AP1 (chamada sem medição, calibração, laço ≈ 1 s, FIFO do perf, espera de temperatura). O que mudou na AP2.

### 4.4 Fórmulas

TODO: MFLOPS, potência, energia, MFLOPS/W (AP1) + speedup S(p) = T(1)/T(p) e eficiência E(p) = S(p)/p.

### 4.5 Qualidade dos dados

TODO: execuções válidas, temperatura inicial, frequência (cycles ÷ tempo), variação entre execuções.

## 5. Pragmas e alterações em relação à AP1

TODO: diff do kernel (`omp parallel for schedule(runtime) firstprivate(k)` + `omp simd aligned`), flag `-fopenmp`, cópia local dos coeficientes (por quê), CLI com threads.

## 6. Corretude

TODO: testes com N = 3, 64, 65, 1024 × threads 1, 2, 7, 16 × schedules static/dynamic/guided; borda não escrita.

## 7. Resultados

### 7.1 N = 1024

TODO: tabela por threads (tempo, speedup, eficiência, MFLOPS, W, J, MFLOPS/W, misses).

### 7.2 N = 2048

TODO: tabela por threads.

### 7.3 N = 4096

TODO: tabela por threads.

### 7.4 Gráficos

TODO: tempo, speedup, eficiência, potência, energia, MFLOPS/W e misses em função das threads (`figures/`).

## 8. Análise comparativa

### 8.1 Melhor versão da AP1 × OpenMP

TODO: `simd_aligned` (1 thread) × melhor configuração OpenMP, por N.

### 8.2 Escalabilidade

TODO: speedup e eficiência; regiões P-cores, HT e E-cores.

### 8.3 Memória e cache

TODO: N = 1024 cabe na L3 (8 MB); N = 2048/4096 não cabem; limite de banda de memória.

### 8.4 Custo do OpenMP

TODO: fork/join em chamadas curtas (N = 1024); desbalanceamento P × E com `static`.

### 8.5 Watts × joules

TODO: potência sobe com threads, energia por chamada pode cair; melhor ponto de MFLOPS/W.

## 9. Limitações

TODO

## 10. Conclusões

TODO

## 11. Como reproduzir

TODO: `make parallel-build`, `make parallel-test`, `make parallel-run`, `python3 scripts/compile_data.py`, scripts de análise.
