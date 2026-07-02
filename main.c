#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int qtd_blocos_disco = 8;
size_t conversao_para_bytes = 4096 * 4;

void criar_imagem_de_disco() {
    size_t tamanho_em_bytes = conversao_para_bytes;
    char nome_do_arquivo[] = "disco.img";
    char comando[100];
    
    sprintf(comando, "fsutil file createnew %s %d", nome_do_arquivo, tamanho_em_bytes);
    
    printf("Criando imagem de disco...\n");
    int verificar = system(comando);
    if (verificar == 0) {
        printf("Imagem de disco criada com sucesso!\n");
    }
}

unsigned char *ler_bloco_disco(size_t indice_bloco){
    FILE *disco = fopen("disco.img", "rb");
    size_t offset = indice_bloco * 4096;

    unsigned char *buffer = malloc(4096);

    if(disco == NULL){
        printf("> ERRO AO ABRIR O ARQUIVO!\n");
        system("pause");
        return NULL;
    }

    fseek(disco, offset, SEEK_SET);
    fread(buffer, 4096, 1, disco);
    fclose(disco);
    return buffer;
}

int main() {
    unsigned char *resultado = ler_bloco_disco(0);

    if(resultado == NULL){
        return 1;
    }

    printf("Resultado: ");
    for(int i = 0; i < 46; i++){
        printf("%02x ", resultado[i]);
    }

    printf("\n");
    system("pause");
    free(resultado);
    return 0;
}