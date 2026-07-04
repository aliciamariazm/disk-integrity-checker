#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/evp.h>

size_t conversao_para_bytes = 4096 * 4; //retorna o tamanho em bytes
typedef struct No{
    unsigned char *hash;
    struct No *esq, *dir;
} no;

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

no *criar_no_merkle(unsigned char *hash){
    no *novo_no = malloc(sizeof(no));
    novo_no->hash = hash;
    novo_no->esq = NULL;
    novo_no->dir = NULL;
    return novo_no; //retorna o endereço do nó criado
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

unsigned char *gerar_hash(unsigned char *dados_do_bloco, size_t tamanho){
    unsigned char *hash = malloc(EVP_MAX_MD_SIZE);
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

    return hash;
}

// =========================================================================
// NOVAS FUNÇÕES: PERSISTÊNCIA DA ÁRVORE E GERENCIAMENTO DE MEMÓRIA
// =========================================================================

// Função recursiva para percorrer a árvore e escrever no arquivo binário
void salvar_no_arquivo(no *n, FILE *arquivo) {
    if (n == NULL) return;
    
    // Salva os 32 bytes do hash deste nó no disco
    fwrite(n->hash, 32, 1, arquivo);
    
    // Desce para os nós filhos recursivamente (Travessia em Pré-ordem)
    salvar_no_arquivo(n->esq, arquivo);
    salvar_no_arquivo(n->dir, arquivo);
}

// Função principal que abre o arquivo e aciona o salvamento recursivo
void salvar_arvore_no_disco(no *raiz, char *nome_arquivo) {
    FILE *arquivo = fopen(nome_arquivo, "wb"); // 'wb' = Write Binary (Sobrescreve se existir)
    if (arquivo == NULL) {
        printf("ERRO: Nao foi possivel criar o arquivo da arvore!\n");
        return;
    }
    
    salvar_no_arquivo(raiz, arquivo);
    fclose(arquivo);
    printf("Arvore salva com sucesso no arquivo: %s\n", nome_arquivo);
}

// Função recursiva para limpar a árvore da memória RAM (MUITO IMPORTANTE PARA O VALGRIND)
void liberar_arvore(no *n) {
    if (n == NULL) return;
    
    liberar_arvore(n->esq);
    liberar_arvore(n->dir);
    
    free(n->hash); // Libera o hash alocado com o malloc do EVP_MAX_MD_SIZE
    free(n);       // Libera a struct do nó em si
}

no *gerar_arvore_do_disco(int qtd_blocos){
    // Array dinâmico temporário para guardar os nós do nível que estamos processando
    no **nivel_atual = malloc(qtd_blocos * sizeof(no*));

    // 1. Nível Base: Lê o disco, gera o hash e constrói as folhas
    for (int i = 0; i < qtd_blocos; i++) {
        unsigned char *dados = ler_bloco_disco(i);
        unsigned char *hash_bloco = gerar_hash(dados, 4096); 
        nivel_atual[i] = criar_no_merkle(hash_bloco);   
        free(dados); // Limpa o bloco da RAM após gerar o hash
    }

    // 2. Subindo os Galhos: Constrói os níveis superiores até sobrar 1 nó (a Raiz)
    int qtd_nos_nivel = qtd_blocos;
    while (qtd_nos_nivel > 1) {
        int metade = (qtd_nos_nivel + 1) / 2; // Arredonda para cima em caso ímpar
        no **proximo_nivel = malloc(metade * sizeof(no*));

        for (int i = 0; i < metade; i++) {
            proximo_nivel[i] = criar_no_merkle(NULL); // Cria o nó pai vazio
            proximo_nivel[i]->esq = nivel_atual[i * 2];

            // O Truque do Ímpar: Emparelha o da direita ou duplica o da esquerda
            if (i * 2 + 1 < qtd_nos_nivel) {
                proximo_nivel[i]->dir = nivel_atual[i * 2 + 1];
            } else {
                proximo_nivel[i]->dir = nivel_atual[i * 2]; 
            }

             // ADAPTAÇÃO 3: Lógica de concatenação dos dois hashes filhos!
            // Criamos um buffer de 64 bytes (32 do esquerdo + 32 do direito)
            unsigned char buffer_concat[64];
            
            // Copia os 32 bytes do filho esquerdo para a primeira metade do buffer
            memcpy(buffer_concat, proximo_nivel[i]->esq->hash, 32);
            // Copia os 32 bytes do filho direito para a segunda metade do buffer (pula 32 espaços)
            memcpy(buffer_concat + 32, proximo_nivel[i]->dir->hash, 32);

            // Gera o hash do pai passando o buffer concatenado de 64 bytes!
            proximo_nivel[i]->hash = gerar_hash(buffer_concat, 64);
        }

        free(nivel_atual);
        nivel_atual = proximo_nivel;
        qtd_nos_nivel = metade;
    }

    no *raiz = nivel_atual[0];
    free(nivel_atual); // Libera o array temporário
    return raiz;
}
// =========================================================================

int main() {
    printf("--- INICIANDO VERIFICADOR DE INTEGRIDADE ---\n");
    
    // 1. Garante que o disco existe (cria um de 4 blocos se não existir)
    criar_imagem_de_disco();
    
    int qtd_blocos = 4; // Lendo 4 blocos do disco criado
    printf("Construindo a Arvore de Merkle para %d blocos...\n", qtd_blocos);
    
    // 2. Constrói a árvore inteira na memória
    no *raiz = gerar_arvore_do_disco(qtd_blocos);
    
    if(raiz != NULL) {
        printf("Hash da Raiz gerado: ");
        for(int i = 0; i < 32; i++){
            printf("%02x", raiz->hash[i]);
        }
        printf("\n");
        
        // 3. Salva a árvore construída no arquivo "arvore.bin"
        salvar_arvore_no_disco(raiz, "arvore.bin");
        
        // 4. Limpa toda a estrutura da memória para evitar Memory Leaks
        liberar_arvore(raiz);
        printf("Memoria liberada com sucesso.\n");
    }
    
    system("pause");
    return 0;
}

