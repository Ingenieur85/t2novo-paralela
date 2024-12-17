/*
Fabiano A. de Sá Filho
GRR: 20223831
*/

#include <stdio.h>

void verifica_particoes(long long *Input, int n, long long *P, int np, 
                        long long *Output, unsigned int *Pos) {
    int faixa = 0;  // Current faixa
    int correto = 1;

    for (int i = 0; i < n; i++) {
        while (faixa < np - 1 && Output[i] >= P[faixa]) {
            faixa++;
        }

        if ((faixa == 0 && Output[i] >= P[faixa]) || 
            (faixa > 0 && (Output[i] < P[faixa - 1] || Output[i] >= P[faixa]))) {
            correto = 0;
            printf("Erro no elemento Output[%d] = %lld\n", i, Output[i]);
        }
    }

    if (correto) {
        printf("===> particionamento CORRETO\n");
    } else {
        printf("===> particionamento COM ERROS\n");
    }
}