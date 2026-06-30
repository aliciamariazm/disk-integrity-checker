#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void criar_imagem_de_disco() {
    int tamanho_em_bytes = 16384;
    char nome_do_arquivo[] = "disco";
    char comando[100];
    
    sprintf(comando, "fsutil file createnew %s.img %d", nome_do_arquivo, tamanho_em_bytes);
    
    printf("Criando imagem de disco...\n");
    int verificar = system(comando);
    if (verificar == 0) {
        printf("Imagem de disco criada com sucesso!\n");
    }
}

int main() {
    //criar_imagem_de_disco();

    return 0;
}