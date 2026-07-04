#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/evp.h>

size_t conversao_para_bytes = 4096 * 4; //retorna o tamanho em bytes

typedef struct No{
    unsigned char *hash;
    struct No *esq, *dir;
    int duplicado; // indica se "dir" é uma cópia do ponteiro "esq"
                    // (nó ímpar duplicado). Evita double free em liberar_arvore().
    int qtd_folhas; // NOVO: quantos blocos de disco estao "por baixo" deste no.
                     // 1 para folhas, soma dos filhos para nos internos.
                     // Usado para navegar da raiz ate uma folha especifica sem
                     // precisar percorrer a arvore inteira nem guardar indices.
    int verificado;  // NOVO: cache de verificacao. Vira 1 quando o hash deste
                      // no ja foi conferido nesta execucao, evitando recalcular
                      // o mesmo ramo em leituras verificadas futuras (assim
                      // como o dm-verity cacheia niveis intermediarios da arvore).
} no;

void criar_imagem_de_disco() {
    const char *nome_do_arquivo = "disco.img";

    FILE *teste = fopen(nome_do_arquivo, "rb");
    if (teste != NULL) {
        fclose(teste);
        printf("Imagem de disco ja existe, mantendo o conteudo atual.\n");
        return;
    }

    printf("Criando imagem de disco...\n");
    FILE *disco = fopen(nome_do_arquivo, "wb");
    if (disco == NULL) {
        printf("ERRO: nao foi possivel criar a imagem de disco.\n");
        return;
    }

    unsigned char bloco_vazio[4096] = {0};
    size_t blocos = conversao_para_bytes / 4096;
    for (size_t i = 0; i < blocos; i++) {
        fwrite(bloco_vazio, 4096, 1, disco);
    }

    fclose(disco);
    printf("Imagem de disco criada com sucesso!\n");
}

no *criar_no_merkle(unsigned char *hash){
    no *novo_no = malloc(sizeof(no));
    if (novo_no == NULL) {
        printf("ERRO: falha ao alocar memoria para o no da arvore.\n");
        exit(1);
    }
    novo_no->hash = hash;
    novo_no->esq = NULL;
    novo_no->dir = NULL;
    novo_no->duplicado = 0;
    novo_no->qtd_folhas = 1; // NOVO: por padrao assume que e uma folha (1 bloco).
                              // Nos internos tem isso recalculado logo depois de
                              // ganharem esq/dir (ver gerar_arvore_do_disco).
    novo_no->verificado = 0; // NOVO
    return novo_no;
}

unsigned char *ler_bloco_disco(size_t indice_bloco){

    FILE *disco = fopen("disco.img", "rb");
    if(disco == NULL){
        printf("> ERRO AO ABRIR O ARQUIVO!\n");
        return NULL;
    }

    size_t offset = indice_bloco * 4096;

    unsigned char *buffer = calloc(1, 4096);
    if (buffer == NULL) {
        fclose(disco);
        return NULL;
    }

    fseek(disco, offset, SEEK_SET);
    fread(buffer, 1, 4096, disco);
    fclose(disco);

    return buffer;
}

unsigned char *gerar_hash(unsigned char *dados_do_bloco, size_t tamanho){
    EVP_MD_CTX *contexto = EVP_MD_CTX_new();
    if(contexto == NULL){
        printf("[GERAR_HASH] > Erro ao criar o contexto.\n");
        return NULL;
    }

    unsigned char *hash = malloc(EVP_MAX_MD_SIZE);
    unsigned int tamanho_do_hash;

    EVP_DigestInit_ex(contexto, EVP_sha256(), NULL);
    EVP_DigestUpdate(contexto, dados_do_bloco, tamanho);
    EVP_DigestFinal_ex(contexto, hash, &tamanho_do_hash);
    EVP_MD_CTX_free(contexto);

    return hash;
}

// =========================================================================
// PERSISTÊNCIA DA ÁRVORE E GERENCIAMENTO DE MEMÓRIA
// =========================================================================

void salvar_no_arquivo(no *n, FILE *arquivo) {
    if (n == NULL) return;

    fwrite(n->hash, 32, 1, arquivo);

    salvar_no_arquivo(n->esq, arquivo);
    if (!n->duplicado) {
        salvar_no_arquivo(n->dir, arquivo);
    }
}

void salvar_arvore_no_disco(no *raiz, char *nome_arquivo) {
    FILE *arquivo = fopen(nome_arquivo, "wb");
    if (arquivo == NULL) {
        printf("ERRO: Nao foi possivel criar o arquivo da arvore!\n");
        return;
    }

    salvar_no_arquivo(raiz, arquivo);
    fclose(arquivo);
    printf("Arvore salva com sucesso no arquivo: %s\n", nome_arquivo);
}

void liberar_arvore(no *n) {
    if (n == NULL) return;

    liberar_arvore(n->esq);
    if (!n->duplicado) {
        liberar_arvore(n->dir);
    }

    free(n->hash);
    free(n);
}

no *gerar_arvore_do_disco(int qtd_blocos){
    no **nivel_atual = malloc(qtd_blocos * sizeof(no*));
    if (nivel_atual == NULL) {
        printf("ERRO: falha ao alocar nivel_atual.\n");
        exit(1);
    }

    // 1. Nível Base: Lê o disco, gera o hash e constrói as folhas
    for (int i = 0; i < qtd_blocos; i++) {
        unsigned char *dados = ler_bloco_disco(i);
        if (dados == NULL) {
            printf("ERRO FATAL: nao foi possivel ler o bloco %d do disco.\n", i);
            for (int j = 0; j < i; j++) {
                free(nivel_atual[j]->hash);
                free(nivel_atual[j]);
            }
            free(nivel_atual);
            exit(1);
        }
        unsigned char *hash_bloco = gerar_hash(dados, 4096);
        nivel_atual[i] = criar_no_merkle(hash_bloco);
        free(dados);
    }

    // 2. Subindo os Galhos: Constrói os níveis superiores até sobrar 1 nó (a Raiz)
    int qtd_nos_nivel = qtd_blocos;
    while (qtd_nos_nivel > 1) {
        int metade = (qtd_nos_nivel + 1) / 2;
        no **proximo_nivel = malloc(metade * sizeof(no*));

        for (int i = 0; i < metade; i++) {
            proximo_nivel[i] = criar_no_merkle(NULL);
            proximo_nivel[i]->esq = nivel_atual[i * 2];

            if (i * 2 + 1 < qtd_nos_nivel) {
                proximo_nivel[i]->dir = nivel_atual[i * 2 + 1];
            } else {
                proximo_nivel[i]->dir = nivel_atual[i * 2];
                proximo_nivel[i]->duplicado = 1;
            }

            // NOVO: propaga quantos blocos ficam "por baixo" deste no. Se for
            // duplicado, o "dir" e o mesmo no que o "esq" (mesmos blocos),
            // entao nao soma de novo - senao qtd_folhas ficaria contada em dobro.
            proximo_nivel[i]->qtd_folhas = proximo_nivel[i]->esq->qtd_folhas +
                (proximo_nivel[i]->duplicado ? 0 : proximo_nivel[i]->dir->qtd_folhas);

            unsigned char buffer_concat[64];
            memcpy(buffer_concat, proximo_nivel[i]->esq->hash, 32);
            memcpy(buffer_concat + 32, proximo_nivel[i]->dir->hash, 32);

            proximo_nivel[i]->hash = gerar_hash(buffer_concat, 64);
        }

        free(nivel_atual);
        nivel_atual = proximo_nivel;
        qtd_nos_nivel = metade;
    }

    no *raiz = nivel_atual[0];
    free(nivel_atual);
    return raiz;
}

// =========================================================================
// LEITURA VERIFICADA (estilo dm-verity)
// =========================================================================

// Navega da raiz ate a folha do bloco "indice_alvo", guardando cada no
// visitado em "caminho" (indice 0 = raiz, ultimo indice = folha). Em cada
// bifurcacao, usa "qtd_folhas" da subarvore esquerda para decidir se o bloco
// procurado esta a esquerda ou a direita - sem precisar percorrer a arvore
// inteira nem guardar um indice em cada no.
no *buscar_caminho_ate_folha(no *atual, int indice_alvo, no **caminho, int *profundidade) {
    caminho[(*profundidade)++] = atual;

    if (atual->esq == NULL && atual->dir == NULL) {
        return atual; // chegou numa folha
    }

    if (indice_alvo < atual->esq->qtd_folhas) {
        return buscar_caminho_ate_folha(atual->esq, indice_alvo, caminho, profundidade);
    } else {
        return buscar_caminho_ate_folha(atual->dir, indice_alvo - atual->esq->qtd_folhas, caminho, profundidade);
    }
}

// ler_bloco_verificado(): "leitura verificada" no estilo dm-verity.
//
// Le um bloco do disco (fonte NAO confiavel, pode ter sido alterada por um
// atacante) e confirma sua integridade recalculando o caminho de hashes ate
// a raiz da arvore de Merkle (mantida em memoria desde a construcao, essa
// sim confiavel). Qualquer no no caminho que nao bater com o valor esperado
// indica adulteracao - seja do bloco em si, seja de algum no intermediario.
//
// Parametros:
//   raiz          - raiz da arvore ja construida (fonte confiavel de hashes)
//   indice_bloco  - qual bloco do disco queremos ler
//   buffer_saida  - buffer de pelo menos 4096 bytes; so e preenchido se a
//                   verificacao passar
//
// Retorno:
//    1  -> bloco integro, buffer_saida preenchido com os dados
//    0  -> ADULTERACAO detectada (bloco ou algum no do caminho nao bate)
//   -1  -> erro (indice invalido / falha de leitura em disco)
int ler_bloco_verificado(no *raiz, int indice_bloco, unsigned char *buffer_saida) {

    if (raiz == NULL || indice_bloco < 0 || indice_bloco >= raiz->qtd_folhas) {
        printf("[LER_VERIFICADO] > indice de bloco invalido (%d).\n", indice_bloco);
        return -1;
    }

    // 1. Le o bloco "cru" (nao confiavel) direto do disco
    unsigned char *dados = ler_bloco_disco(indice_bloco);
    if (dados == NULL) {
        printf("[LER_VERIFICADO] > falha ao ler o bloco %d do disco.\n", indice_bloco);
        return -1;
    }

    // 2. Localiza o caminho raiz -> folha correspondente a esse bloco
    no *caminho[64]; // 64 niveis cobrem ate 2^64 blocos - mais que suficiente
    int profundidade = 0;
    no *folha = buscar_caminho_ate_folha(raiz, indice_bloco, caminho, &profundidade);

    // 3. Recalcula o hash do bloco lido AGORA e compara com o hash "congelado"
    //    na folha (calculado quando a arvore foi construida, ou seja, quando
    //    o estado do disco ainda era confiavel)
    unsigned char *hash_atual = gerar_hash(dados, 4096);

    if (memcmp(hash_atual, folha->hash, 32) != 0) {
        printf("[LER_VERIFICADO] > ADULTERACAO DETECTADA: bloco %d foi modificado!\n", indice_bloco);
        free(hash_atual);
        free(dados);
        return 0;
    }
    free(hash_atual);

    // 4. Sobe da folha ate a raiz recalculando cada nivel do caminho.
    //    Usa "verificado" como cache: se um no ja foi validado numa leitura
    //    verificada anterior (nesta mesma execucao), o ramo inteiro acima
    //    dele ja esta confirmado e nao precisa ser recalculado de novo.
    for (int i = profundidade - 2; i >= 0; i--) {
        no *pai = caminho[i];

        if (pai->verificado) {
            break;
        }

        unsigned char buffer_concat[64];
        memcpy(buffer_concat, pai->esq->hash, 32);
        memcpy(buffer_concat + 32, pai->dir->hash, 32);

        unsigned char *hash_pai = gerar_hash(buffer_concat, 64);
        int bate = (memcmp(hash_pai, pai->hash, 32) == 0);
        free(hash_pai);

        if (!bate) {
            printf("[LER_VERIFICADO] > ADULTERACAO DETECTADA na arvore (nivel %d, acima do bloco %d)!\n",
                   i, indice_bloco);
            free(dados);
            return 0;
        }

        pai->verificado = 1;
    }

    // 5. Tudo conferido ate a raiz: agora sim copia os dados pra quem chamou
    memcpy(buffer_saida, dados, 4096);
    free(dados);

    return 1;
}

// Auxiliar so pra demonstrar/testar: simula um atacante escrevendo direto
// no disco.img, por fora da arvore de Merkle (que fica intocada em memoria).
void adulterar_bloco_no_disco(int indice_bloco) {
    FILE *disco = fopen("disco.img", "r+b");
    if (disco == NULL) return;
    fseek(disco, indice_bloco * 4096, SEEK_SET);
    unsigned char lixo[] = "--- BLOCO ADULTERADO POR UM ATACANTE ---";
    fwrite(lixo, sizeof(lixo), 1, disco);
    fclose(disco);
}

// =========================================================================

int main() {
    printf("--- INICIANDO VERIFICADOR DE INTEGRIDADE ---\n");

    // criar_imagem_de_disco();

    int qtd_blocos = 4;
    // printf("Construindo a Arvore de Merkle para %d blocos...\n", qtd_blocos);

    no *raiz = gerar_arvore_do_disco(qtd_blocos);

    if(raiz != NULL) {
        printf("Hash da Raiz gerado: ");
        for(int i = 0; i < 32; i++){
            printf("%02x", raiz->hash[i]);
        }
        printf("\n");

        salvar_arvore_no_disco(raiz, "arvore.bin");
        
        unsigned char buffer_leitura[4096];
     
        printf("\n--- TESTE 1: leitura verificada do bloco 0 (disco intacto) ---\n");
        int resultado = ler_bloco_verificado(raiz, 0, buffer_leitura);
        printf("Resultado: %s\n", resultado == 1 ? "OK, bloco integro." : "FALHOU (inesperado!)");
     
        printf("\n--- Simulando um atacante alterando o bloco 2 direto no disco ---\n");
        adulterar_bloco_no_disco(2);
     
        printf("\n--- TESTE 2: leitura verificada do bloco 2 (apos adulteracao) ---\n");
        resultado = ler_bloco_verificado(raiz, 2, buffer_leitura);
        printf("Resultado: %s\n", resultado == 1 ? "OK, bloco integro (inesperado!)" : "Adulteracao detectada como esperado.");
     
        printf("\n--- TESTE 3: bloco 0 continua integro (nao foi mexido) ---\n");
        resultado = ler_bloco_verificado(raiz, 0, buffer_leitura);
        printf("Resultado: %s\n", resultado == 1 ? "OK, bloco integro." : "FALHOU (inesperado!)");
     
        printf("\n--- TESTE 4: indice invalido ---\n");
        resultado = ler_bloco_verificado(raiz, 99, buffer_leitura);
        printf("Resultado: %d (esperado -1)\n", resultado);
        liberar_arvore(raiz);
        printf("\nMemoria liberada com sucesso.\n");
    }   
 

#ifdef _WIN32
    system("pause");
#endif
    return 0;
}