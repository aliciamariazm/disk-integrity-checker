#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/evp.h>

size_t conversao_para_bytes = 4096 * 4; //retorna o tamanho em bytes

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

    fseek(disco, offset, SEEK_SET); //posiciona o cursor
    fread(buffer, 4096, 1, disco);
    fclose(disco);

    return buffer;
}

unsigned char *gerar_hash(unsigned char *dados_do_bloco){
    unsigned char hash[EVP_MAX_MD_SIZE];
    int tamanho_do_hash;
    EVP_MD_CTX *contexto = EVP_MD_CTX_new();

    if(contexto == NULL){
        printf("[GERAR_HASH] > Erro ao criar o contexto.\n");
        return NULL;
    }

    EVP_DigestInit_ex(contexto, EVP_sha256(), NULL);
    EVP_DigestUpdate(contexto, dados_do_bloco, 4096);
    EVP_DigestFinal_ex(contexto, hash, &tamanho_do_hash);
    EVP_MD_CTX_free(contexto);

    unsigned char *result = hash;
    return result;
}

int main() {
    unsigned char *dados_do_bloco = ler_bloco_disco(1);
    unsigned char *hash;

    if(dados_do_bloco == NULL){
        return 1;
    }

    hash = gerar_hash(dados_do_bloco);
    
    printf("Dados do bloco: ");
    for(int i = 0; i < 8; i++){
        printf("%02x ", dados_do_bloco[i]);
    }
    
    printf("\nHash: ");
    for(int i = 0; i < 32; i++){
        printf("%02x ", hash[i]);
    }

    printf("\n");
    system("pause");
    free(dados_do_bloco);
    return 0;
}

/*teste git isabella*/