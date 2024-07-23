#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_COMMAND_ARGUMENTS 100

#define MAX_COMMAND_ARGUMENT_LENGTH 255

#define COMMAND_BUFFER_SIZE (MAX_COMMAND_ARGUMENTS * MAX_COMMAND_ARGUMENT_LENGTH)
// The delimiter between arguments is a space.
#define COMMAND_ARGUMENTS_DELIMITER " "

#define aggiungi_ricetta_HASH 1686
#define rimuovi_ricetta_HASH 1622
#define rifornimento_HASH 1308
#define ordine_HASH 641

#define INITIAL_TABLE_SIZE 5000
#define LOAD_FACTOR_THRESHOLD 0.7

//lotto (nodo, cella hash table)
typedef struct Batch {
    int expiration;
    int quantity;
    int quantityLeft;
    int lastTimeModified;
    struct Batch* next;
} Batch;

// Struttura per il nodo della lista esterna
typedef struct Item {
    char* ingredientKey;
    Batch* list;
    bool isDeleted;
    struct Item* next;
} Item;

//ingrediente
typedef struct Node {
    Item** item;
    int quantity;
    struct Node* next;
} Node;

//lista di ingredienti
typedef struct List {
    Node* head;
} List;

//ricetta
typedef struct Recipe {
    char* name;
    List* ingredientList;
    bool isDeleted;
    int state;
    int lastTimeUpdated;
    int numberOrder;
    struct Recipe* next;
} Recipe;

//lista di lotti (magazzino)
typedef struct RecipeHashTable {
    Recipe** items;            //lotto con scadenza più vicina (dove estraggo)
    int size;                   //dimensione della tabella hash
    int count;                  //numero di ingredienti presenti
} RecipeHashTable;

//lista di lotti (magazzino)
typedef struct HashTable {
    Item** items;            //lotto con scadenza più vicina (dove estraggo)
    int size;                   //dimensione della tabella hash
    int count;                  //numero di ingredienti presenti
} HashTable;

typedef struct Order {
    int quantity;
    Recipe* recipe;
    int weight;
    int time;
    struct Order* next;
} Order;

//queue di ordini in cui vengono messi in ordine di peso. invece a parità di peso in ordine cronologico
//più pesante -> meno pesante
//più vecchio -> più recente (a parità di peso)
typedef struct Queue {
    Order* head;
    Order* tail;
} Queue;

//
//prototipi funzioni
//
void resize(HashTable** table, int currentTime);
void removeBatchFromHashTable(HashTable** table, Item* item, int currentTime);
void insertRecipeInHashTable(RecipeHashTable** table, Recipe* batch);
void resizeRecipeHashTable(RecipeHashTable** table);


//
//funzioni hash
//

unsigned int hashFunction(char* key, int tableSize) {
    unsigned long int value = 5381;
    int c;
    //unsigned int key_len = strlen(key);

    while ((c = *key++)) {
        value = (value * 33 + c);
    }

    return value % tableSize;
}

unsigned int hashFunction2(char* key, int tableSize) {
    unsigned long int value = 0;
    //unsigned int i = 0;
    int c;
    //unsigned int key_len = strlen(key);

    while ((c = *key++)) {
        value = (value + c);
        //i++;
    }

    return value * 37 % tableSize;
}

uint32_t hash(char* string){

    uint32_t hash = 5381;
    int c;
    while((c = *string++)){
        hash = hash + c;
    }
    return hash;
}

int hash_strcmp(char* s1, char* s2){

    uint32_t hash1 = hash(s1);
    uint32_t hash2 = hash(s2);
    if(hash1 == hash2){
        return strcmp(s1, s2);
    }
    return hash1 - hash2;
}


//
//funzioni create
//


Recipe* createRecipe(char* nameRecipe) {
    Recipe* newRecipe = (Recipe*)malloc(sizeof(Recipe));
    List* ingredientList = (List*)malloc(sizeof(List));
    if (newRecipe == NULL || ingredientList == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newRecipe -> name = strndup(nameRecipe, 257);
    ingredientList -> head = NULL;
    newRecipe -> ingredientList = ingredientList;
    newRecipe -> isDeleted = false;
    newRecipe -> state = 0;
    newRecipe -> lastTimeUpdated = -1;
    newRecipe -> numberOrder = 0;

    return newRecipe;
}

RecipeHashTable* createRecipeHashTable() {
    RecipeHashTable* newHashTable = (RecipeHashTable*)malloc(sizeof(RecipeHashTable));
    if (newHashTable == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newHashTable -> items = (Recipe**)malloc(sizeof(Recipe*) * INITIAL_TABLE_SIZE);
    newHashTable -> size = INITIAL_TABLE_SIZE;
    newHashTable -> count = 0;
    //inizializzo tutte le celle dell'hash table a NULL
    for (int i = 0; i < newHashTable -> size; i++) {
        newHashTable -> items[i] = NULL;
    }
    return newHashTable;
}

Order* createOrder(Recipe* recipe, int quantity, int currentTime) {

    Order* newOrder = (Order*)malloc(sizeof(Order));
    if(newOrder == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newOrder -> next = NULL;
    newOrder -> recipe = recipe;
    newOrder -> recipe -> numberOrder++;
    newOrder -> quantity = quantity;
    newOrder -> weight = 0;
    newOrder -> time = currentTime;
    return newOrder;
}

Queue* createQueue() {

    Queue* newQueue = (Queue*)malloc(sizeof(Queue));
    if(newQueue == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newQueue -> head = NULL;
    newQueue -> tail = NULL;
    return newQueue;
}

Batch* createNodeBatch(int expiration, int quantity) {

    Batch* newBatch = (Batch*)malloc(sizeof(Batch));
    if(newBatch == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newBatch -> expiration = expiration;
    newBatch -> quantity = quantity;
    newBatch -> quantityLeft = quantity;
    newBatch -> next = NULL;
    newBatch -> lastTimeModified = -1;

    return newBatch;
}

Item* createItem(char* key) {
    Item* item = (Item*)malloc(sizeof(Item));
    if (item == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    item -> ingredientKey = strndup(key, 257);
    item -> list = NULL;
    item -> isDeleted = false;

    return item;
}

HashTable* createHashTable() {
    HashTable* newHashTable = (HashTable*)malloc(sizeof(HashTable));
    if (newHashTable == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newHashTable -> items = (Item**)malloc(sizeof(Item*) * INITIAL_TABLE_SIZE);
    newHashTable -> size = INITIAL_TABLE_SIZE;
    newHashTable -> count = 0;
    //inizializzo tutte le celle dell'hash table a NULL
    for (int i = 0; i < newHashTable -> size; i++) {
        newHashTable -> items[i] = NULL;
    }
    return newHashTable;
}

Node* createNodeIngredient(char* nameIngredient, int quantity) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    //newNode -> ingredientName = strndup(nameIngredient, 257);
    newNode -> item = NULL;
    newNode -> quantity = quantity;
    newNode -> next = NULL;
    return newNode;
}

//
//funzioni free
//

void freeIngredientList(List* list){

    Node* currentIngredient = list -> head;
    Node* temp;
    while(currentIngredient != NULL){
        temp = currentIngredient -> next;
        //free(currentIngredient -> ingredientName);
        free(currentIngredient);
        currentIngredient = temp;
    }
}

void freeRecipeInHashTable(RecipeHashTable* table){

    for (int i = 0; i < table -> size; i++) {
        if(table -> items[i] != NULL){
            freeIngredientList(table -> items[i] -> ingredientList);
            free(table -> items[i] -> ingredientList);
            free(table -> items[i] -> name);
        }
        free(table -> items[i]);
    }
}

void freeOrders(Queue* queue){

    Order* currentOrder = queue -> head;
    Order* temp;

    while(currentOrder != NULL){
        temp = currentOrder->next;
        //freeIngredientList(currentOrder -> ingredientList);
        //free(currentOrder -> recipeName);
        free(currentOrder);
        currentOrder = temp;
    }
}

void freeBatch(Batch* head){

    Batch* currentBatch = head;
    Batch* temp;
    while(currentBatch != NULL){
        temp = currentBatch -> next;
        //if(currentBatch -> ingredient != NULL)
         //   free(currentBatch -> ingredient);
        free(currentBatch);
        currentBatch = temp;
    }
}

void freeHashTable(HashTable* table){

    for (int i = 0; i < table -> size; i++) {
        if(table -> items[i] != NULL){
            freeBatch(table -> items[i] -> list);
            free(table -> items[i] -> ingredientKey);
        }
        free(table -> items[i]);
    }
}

//
//funzioni append, insert
//

void appendIngredientToRecipe(Node* newNode, List* list){

    if(list -> head == NULL) {
        list -> head = newNode;
        newNode -> next = NULL;
        return;
    }

    Node* temp = list -> head;
    Node* last = NULL;

    while(temp != NULL) {
        int cmp = strcmp((*temp -> item) -> ingredientKey , (*newNode -> item) -> ingredientKey);
        if(cmp < 0) {
            last = temp;
            temp = temp -> next;

        }else if(cmp == 0){
            //ingrediente già presente nella lista
            return;

        }else {
            //posiziono il newNode prima del nodo temp
            if(last != NULL) {
                //inserisco prima di temp e dopo last
                last -> next = newNode;
                newNode -> next = temp;
            }else {
                //inserisco in testa
                newNode -> next = list -> head;
                list -> head = newNode;
            }
            return;
        }
    }
    //arrivo qui solo se il nuovo ingrediente va inserito in coda
    if(last != NULL) {
        //inserimento in coda con X elementi in lista
        last -> next = newNode;
        newNode -> next = NULL;
    }else {
        //inserimento in coda con solo un elemento in lista
        list -> head -> next = newNode;
        newNode -> next = NULL;
    }
}

void forceInsertRecipeInHashTable(RecipeHashTable** table, Recipe* newBatch) {

    char* ingredientKey = newBatch -> name;

    unsigned int index = hashFunction(ingredientKey, (*table)->size);
    //unsigned int step = hashFunction2(ingredientKey, (*table)->size);

    for (int i = 0; i < (*table) -> size; i++) {
        int tryIndex = (index + i) % (*table)->size;

        if ((*table)->items[tryIndex] == NULL) {
            //newItem = createItem(ingredientKey);
            //newItem -> list = newBatch;
            //newBatch -> next = NULL;
            (*table)->items[tryIndex] = newBatch;
            (*table)->count++;
            return;
        }

        //controllo se la cella è stata cancellata
        if ((*table)->items[tryIndex]->isDeleted) {
            //se la cella è stata cancellata devo inserire il nuovo batch nella cella in cima
            //libero memoria occupata dal vecchio batch
            //free((*table)->items[tryIndex]->list->ingredient);
            freeIngredientList((*table)->items[tryIndex]->ingredientList);
            free((*table)->items[tryIndex]->ingredientList);
            free((*table)->items[tryIndex]->name);
            free((*table)->items[tryIndex]);
            (*table)->items[tryIndex] = newBatch;
            (*table)->items[tryIndex]->isDeleted = false;
            //libero memoria della vecchia stringa
            //(*table)->items[tryIndex]->ingredientKey = strndup(ingredientKey, 257);
            //newBatch -> next = NULL;
            (*table)->count++;
            return;
        }

        //controllo se la key della cella è uguale a ingredientKey
        if (hash_strcmp((*table) -> items[tryIndex] -> name, ingredientKey) == 0) {

            //controllo se la cella è stata cancellata
            if ((*table)->items[tryIndex]->isDeleted) {
                //se la cella è stata cancellata devo inserire il nuovo batch nella cella in cima
                //libero memoria occupata dal vecchio batch
                //free((*table)->items[tryIndex]->list->ingredient);
                freeIngredientList((*table)->items[tryIndex]->ingredientList);
                free((*table)->items[tryIndex]->ingredientList);
                free((*table)->items[tryIndex]->name);
                free((*table)->items[tryIndex]);
                (*table)->items[tryIndex] = newBatch;
                (*table)->items[tryIndex]->isDeleted = false;
                //libero memoria della vecchia stringa
                //(*table)->items[tryIndex]->ingredientKey = strndup(ingredientKey, 257);
                //newBatch -> next = NULL;
                (*table)->count++;
                return;
            }
            //ricetta già presente nella hashTable
            return;
        }
    }
    //resize
    resizeRecipeHashTable(table);
    forceInsertRecipeInHashTable(table, newBatch);
}

void insertRecipeInHashTable(RecipeHashTable** table, Recipe* newRecipe) {

    double loadFactor = (double)(*table) -> count / (double)(*table) -> size;
    if(loadFactor > LOAD_FACTOR_THRESHOLD) {
        resizeRecipeHashTable(table);
    }

    char* ingredientKey = newRecipe -> name;

    unsigned int index = hashFunction(ingredientKey, (*table)->size);
    //unsigned int step = hashFunction2(ingredientKey, (*table)->size);

    for (int i = 0; i < (*table) -> size; i++) {
        int tryIndex = (index + i) % (*table)->size;

        if ((*table)->items[tryIndex] == NULL) {
            //newItem = createItem(ingredientKey);
            //newItem -> list = newBatch;
            //newBatch -> next = NULL;
            (*table)->items[tryIndex] = newRecipe;
            (*table)->count++;
            return;
        }

        //controllo se la key della cella è uguale a ingredientKey
        if (strcmp((*table) -> items[tryIndex] -> name, ingredientKey) == 0) {

            //controllo se la cella è stata cancellata
            if ((*table)->items[tryIndex]->isDeleted) {
                //se la cella è stata cancellata devo inserire il nuovo batch nella cella in cima
                //libero memoria occupata dal vecchio batch
                //free((*table)->items[tryIndex]->list->ingredient);
                freeIngredientList((*table)->items[tryIndex]->ingredientList);
                free((*table)->items[tryIndex]->ingredientList);
                free((*table)->items[tryIndex]->name);
                free((*table)->items[tryIndex]);
                (*table)->items[tryIndex] = newRecipe;
                (*table)->items[tryIndex]->isDeleted = false;
                //libero memoria della vecchia stringa
                //(*table)->items[tryIndex]->ingredientKey = strndup(ingredientKey, 257);
                //newBatch -> next = NULL;
                (*table)->count++;
                return;
            }

            //ricetta già presente nella hashTable. non faccio nulla
            return;
        }
    }
    forceInsertRecipeInHashTable(table, newRecipe);
}

void appendToInternalList(HashTable** table, Item** item, Batch* newBatch, int currentTime, int index) {

    if ((*item) -> list == NULL) {
        (*item) -> list = newBatch;
        newBatch -> next = NULL;
        return;
    }

    Batch* currentBatch = (*item) -> list;
    Batch* last = NULL;

    while(currentBatch != NULL) {

        if(currentBatch -> expiration <= newBatch -> expiration) {
            last = currentBatch;
            currentBatch = currentBatch -> next;
        }else {
            if(last == NULL) {
                Batch* temp = (*item) -> list;
                (*item) -> list = newBatch;
                newBatch -> next = temp;
                //*head = newBatch;
            }else {
                last -> next = newBatch;
                newBatch -> next = currentBatch;
            }
            return;
        }
    }
    //inserimento in coda dopo aver scorso tutta la lista
    if(last == NULL) {
        //inserimento in coda con un solo elemento (non dovrebbe succedere)
        printf("error in appendToInternalList");
    }else {
        last -> next = newBatch;
        newBatch -> next = NULL;
    }
}

//l'unica differenza tra forceInsertInHashTable e insertBatchInHashTable è che forceInsertInHashTable posiziona
//il nuovo batch anche se la cella è stata cancellata ma l'ingredientKey è diverso. invece insertBatchInHashTable
//non posiziona il nuovo batch se la cella è stata cancellata e l'ingredientKey è diverso perchè
//altrimenti rischio di avere ingredienti dello stesso tipo in celle diverse
void forceInsertInHashTable(HashTable** table, Batch* newBatch, int currentTime, char* ingredientKey) {

    Item* newItem = NULL;

    unsigned int index = hashFunction(ingredientKey, (*table)->size);
    //unsigned int step = hashFunction2(ingredientKey, (*table)->size);

    for (int i = 0; i < (*table) -> size; i++) {
        int tryIndex = (index + i) % (*table)->size;

        if ((*table)->items[tryIndex] == NULL) {
            newItem = createItem(ingredientKey);
            newItem -> list = newBatch;
            newBatch -> next = NULL;
            (*table)->items[tryIndex] = newItem;
            (*table)->count++;
            return;
        }

        //controllo se la cella è stata cancellata e inserisco il batch nella cella in cima
        if ((*table)->items[tryIndex]->isDeleted) {
            //se la cella è stata cancellata devo inserire il nuovo batch nella cella in cima
            //libero memoria occupata dal vecchio batch
            //free((*table)->items[tryIndex]->list->ingredient);
            freeBatch((*table)->items[tryIndex]->list);
            (*table)->items[tryIndex]->list = newBatch;
            (*table)->items[tryIndex]->isDeleted = false;
            //libero memoria della vecchia stringa
            free((*table)->items[tryIndex]->ingredientKey);
            (*table)->items[tryIndex]->ingredientKey = strndup(ingredientKey, 257);
            newBatch -> next = NULL;
            (*table)->count++;
            return;
        }

        //controllo se la key della cella è uguale a ingredientKey
        if (hash_strcmp((*table) -> items[tryIndex] -> ingredientKey, ingredientKey) == 0) {

            //controllo se la cella è stata cancellata
            if ((*table)->items[tryIndex]->isDeleted) {
                //se la cella è stata cancellata devo inserire il nuovo batch nella cella in cima
                //libero memoria occupata dal vecchio batch
                //free((*table)->items[tryIndex]->list->ingredient);
                freeBatch((*table)->items[tryIndex]->list);
                (*table)->items[tryIndex]->list = newBatch;
                (*table)->items[tryIndex]->isDeleted = false;
                //libero memoria della vecchia stringa
                free((*table)->items[tryIndex]->ingredientKey);
                (*table)->items[tryIndex]->ingredientKey = strndup(ingredientKey, 257);
                newBatch -> next = NULL;
                (*table)->count++;
                return;
            }

            //se la cella non è stata cancellata devo inserire il nuovo batch nella lista interna
            //Batch* headBatch = (*table) -> items[tryIndex] -> list;
            //scorro la lista interna e inserisco il nuovo batch in ordine di expiration
            appendToInternalList(table, &(*table) -> items[tryIndex], newBatch, currentTime, tryIndex);
            return;
        }
    }
    resize(table, currentTime);
    forceInsertInHashTable(table, newBatch, currentTime, ingredientKey);
}

void insertItemInHashTable(HashTable** table, int currentTime, Node* ingredientNode, char* ingredientKey) {

    double loadFactor = (double)(*table) -> count / (double)(*table) -> size;
    if(loadFactor > LOAD_FACTOR_THRESHOLD) {
        resize(table, currentTime);
    }
    //char* ingredientKey = newBatch -> ingredient;

    unsigned int index = hashFunction(ingredientKey, (*table)->size);
    //unsigned int step = hashFunction2(ingredientKey, (*table)->size);

    for (int i = 0; i < (*table) -> size; i++) {
        int tryIndex = (index + i) % (*table)->size;

        if ((*table)->items[tryIndex] == NULL) {
            Item* newItem = createItem(ingredientKey);
            //newItem -> list = newBatch;
            //newBatch -> next = NULL;
            (*table)->items[tryIndex] = newItem;
            (*table)->count++;
            newItem -> list = NULL;
            ingredientNode -> item = &(*table)->items[tryIndex];
            return;
        }

        //controllo se la key della cella è uguale a ingredientKey
        if (strcmp((*table) -> items[tryIndex] -> ingredientKey, ingredientKey) == 0) {

            //se la cella non è stata cancellata devo inserire il nuovo batch nella lista interna
            //Batch* headBatch = (*table) -> items[tryIndex] -> list;
            //scorro la lista interna e inserisco il nuovo batch in ordine di expiration
            //appendToInternalList(table, &headBatch, newBatch, currentTime, tryIndex);
            ingredientNode -> item = &(*table) -> items[tryIndex];
            return;
        }
    }
}

void insertBatchInHashTable(HashTable** table, Batch* newBatch, int currentTime, char* ingredientKey) {

    //se il batch è scaduto non lo inserisco
    if(newBatch -> expiration <= currentTime)
        return;

    double loadFactor = (double)(*table) -> count / (double)(*table) -> size;
    if(loadFactor > LOAD_FACTOR_THRESHOLD) {
        resize(table, currentTime);
    }
    Item* newItem = NULL;
    //char* ingredientKey = newBatch -> ingredient;

    unsigned int index = hashFunction(ingredientKey, (*table)->size);
    //unsigned int step = hashFunction2(ingredientKey, (*table)->size);

    for (int i = 0; i < (*table) -> size; i++) {
        int tryIndex = (index + i) % (*table)->size;

        if ((*table)->items[tryIndex] == NULL) {
            newItem = createItem(ingredientKey);
            newItem -> list = newBatch;
            newBatch -> next = NULL;
            (*table)->items[tryIndex] = newItem;
            (*table)->count++;
            return;
        }

        //controllo se la key della cella è uguale a ingredientKey
        if (hash_strcmp((*table) -> items[tryIndex] -> ingredientKey, ingredientKey) == 0) {

            //controllo se la cella è stata cancellata
            if ((*table)->items[tryIndex]->isDeleted) {
                //se la cella è stata cancellata devo inserire il nuovo batch nella cella in cima
                //libero memoria occupata dal vecchio batch
                //free((*table)->items[tryIndex]->list->ingredient);
                freeBatch((*table)->items[tryIndex]->list);
                (*table)->items[tryIndex]->list = newBatch;
                (*table)->items[tryIndex]->isDeleted = false;
                //libero memoria della vecchia stringa
                free((*table)->items[tryIndex]->ingredientKey);
                (*table)->items[tryIndex]->ingredientKey = strndup(ingredientKey, 257);
                newBatch -> next = NULL;
                (*table)->count++;
                return;
            }

            //se la cella non è stata cancellata devo inserire il nuovo batch nella lista interna
            //Batch* headBatch = (*table) -> items[tryIndex] -> list;
            //scorro la lista interna e inserisco il nuovo batch in ordine di expiration
            appendToInternalList(table, &(*table) -> items[tryIndex], newBatch, currentTime, tryIndex);
            return;
        }
    }
    forceInsertInHashTable(table, newBatch, currentTime, ingredientKey);
}

void resize(HashTable** table, int currentTime) {

    int newSize = (*table) -> size * 2;
    //Batch* newBatch = NULL;
    //inizializzo nuovi items a NULL
    Item** newItems = (Item**)malloc(newSize * sizeof(Item*));

    for (int i = 0; i < newSize; i++) {
        newItems[i] = NULL;
    }

    Item** oldItems = (*table) -> items;
    int oldSize = (*table) -> size;

    // Aggiorna la tabella con la nuova size, count e items
    (*table) -> size = newSize;
    (*table) -> count = 0;
    (*table) -> items = newItems;

    for (int i = 0; i < oldSize; i++) {
        if (oldItems[i] != NULL && !oldItems[i] -> isDeleted) {
            Item* item = oldItems[i];
            Batch* batch = item -> list;
            Batch* temp;

            while (batch != NULL) {
                temp = batch -> next;
                batch -> next = NULL;
                //newBatch = createNodeBatch(batch -> ingredient, batch -> expiration, batch -> quantity);
                insertBatchInHashTable(table, batch, currentTime, item -> ingredientKey);
                batch = temp;
            }
        }
        if(oldItems[i] != NULL) {
            free(oldItems[i] -> ingredientKey);
        }
        free(oldItems[i]);
    }

    free(oldItems);
}

void resizeRecipeHashTable(RecipeHashTable** table) {

    int newSize = (*table) -> size * 2;
    //Batch* newBatch = NULL;
    //inizializzo nuovi items a NULL
    Recipe** newItems = (Recipe**)malloc(newSize * sizeof(Recipe*));

    for (int i = 0; i < newSize; i++) {
        newItems[i] = NULL;
    }

    Recipe** oldItems = (*table) -> items;
    int oldSize = (*table) -> size;

    // Aggiorna la tabella con la nuova size, count e items
    (*table) -> size = newSize;
    (*table) -> count = 0;
    (*table) -> items = newItems;

    for (int i = 0; i < oldSize; i++) {
        if (oldItems[i] != NULL && !oldItems[i] -> isDeleted) {
            Recipe* recipe = oldItems[i];
            //newBatch = createNodeBatch(batch -> ingredient, batch -> expiration, batch -> quantity);
            insertRecipeInHashTable(table, recipe);
        }
    }
    free(oldItems);
}

void appendOrderInQueue(Order* newOrder, Queue* waitQueue) {

    if(waitQueue -> head == NULL) {
        waitQueue -> tail = newOrder;
        waitQueue -> head = waitQueue -> tail;
        newOrder -> next = NULL;
        return;
    }

    waitQueue -> tail -> next = newOrder;
    waitQueue -> tail = newOrder;
    newOrder -> next = NULL;
}

void appendOrderInReadyQueue(Order* newOrder, Queue* readyQueue) {

    if(readyQueue -> head == NULL) {
        readyQueue -> tail = newOrder;
        readyQueue -> head = newOrder;
        newOrder -> next = NULL;
        return;
    }

    Order* currentOrder = readyQueue -> head;
    Order* last = NULL;
    int orderTime = newOrder -> time;

    while (currentOrder != NULL) {
        if(currentOrder -> time > orderTime) {
            if(last == NULL) {
                newOrder -> next = readyQueue -> head;
                readyQueue -> head = newOrder;
            }else {
                last -> next = newOrder;
                newOrder -> next = currentOrder;
            }
            return;
        }else {
            last = currentOrder;
            currentOrder = currentOrder -> next;
        }
    }
    //inserimento in coda
    if(last == NULL) {
        //inserimento in coda con un solo nodo presente nella queue (in teoria non succede perchè last è sempre diverso da NULL)
        readyQueue -> head -> next = newOrder;
        readyQueue -> tail = newOrder;
        newOrder -> next = NULL;
    }else {
        //inserimento in coda con più nodi presenti nella queue
        last -> next = newOrder;
        newOrder -> next = NULL;
        readyQueue -> tail = newOrder;
    }
}


void appendOrderInVanQueue(Order* newOrder, Queue* vanQueue) {

    if(vanQueue -> head == NULL) {
        vanQueue -> tail = newOrder;
        vanQueue -> head = newOrder;
        newOrder -> next = NULL;
        return;
    }

    Order* currentOrder = vanQueue -> head;
    Order* last = NULL;
    int weight = newOrder -> weight;

    while (currentOrder != NULL) {
        if(currentOrder -> weight < weight) {
            if(last == NULL) {
                newOrder -> next = vanQueue -> head;
                vanQueue -> head = newOrder;
            }else {
                last -> next = newOrder;
                newOrder -> next = currentOrder;
            }
            return;
        }else {
            last = currentOrder;
            currentOrder = currentOrder -> next;
        }
    }
    //inserimento in coda
    if(last == NULL) {
        //inserimento in coda con un solo nodo presente nella queue (in teoria non succede perchè last è sempre diverso da NULL)
        vanQueue -> head -> next = newOrder;
        vanQueue -> tail = newOrder;
        newOrder -> next = NULL;
    }else {
        //inserimento in coda con più nodi presenti nella queue
        last -> next = newOrder;
        newOrder -> next = NULL;
        vanQueue -> tail = newOrder;
    }
}


void removeOrderInQueue(Queue* queue) {

    if(queue -> head != NULL) {
        Order* head = queue -> head;
        queue -> head = head -> next;
        if(queue -> head == NULL)
            queue -> tail = NULL;
        free(head);
    }
}


//
//funzioni check
//

Recipe* checkIfRecipeIsPresentInHashTable(char* ingredientKey, RecipeHashTable** table) {

    unsigned int index = hashFunction(ingredientKey, (*table)->size);
    //unsigned int step = hashFunction2(ingredientKey, (*table)->size);

    for (int i = 0; i < (*table) -> size; i++) {
        int tryIndex = (index + i) % (*table)->size;

        if ((*table)->items[tryIndex] == NULL) {
            return NULL;
        }

        //controllo se la key della cella è uguale a ingredientKey
        if (strcmp((*table) -> items[tryIndex] -> name, ingredientKey) == 0) {

            //controllo se la cella è stata cancellata
            if ((*table)->items[tryIndex]->isDeleted) {
                return NULL;
            }
            //ricetta già presente nella hashTable
            return (*table) -> items[tryIndex];
        }
    }
    return NULL;
}

int lastTimeModified = 0;

void removeBatchFromHashTable(HashTable** table, Item* item, int currentTime) {

    Batch* lastBatch = NULL;
    Batch* currentBatch = item -> list;
    Batch* temp;

    while(currentBatch != NULL) {
        //se ingrediente scaduto lo pongo a 0 così che poi lo elimino sotto
        /*if(currentBatch -> expiration <= currentTime) {
            currentBatch -> quantityLeft = -1;
        }*/
        if(currentBatch -> lastTimeModified == lastTimeModified || currentBatch -> expiration <= currentTime) {

            if(currentBatch -> quantityLeft <= 0) {
                currentBatch -> quantityLeft = -1;
                currentBatch -> quantity = 0;
                //currentBatch -> quantity = currentBatch -> quantityLeft;
                //devo cancellare il batch

                //elimino il batch !!
                if(lastBatch == NULL && currentBatch -> next == NULL) {
                    //cancello testa della lista interna senza che ci siano altri elementi quindi pongo isDeleted a true
                    item -> isDeleted = true;
                    //(*table) -> items[index] -> list -> next == NULL;
                    (*table) -> count--;
                    temp = NULL;
                    /*(*currentBatch) -> quantity = 0;
                    (*currentBatch) -> quantityLeft = 0;*/
                    //Batch** temp = currentBatch;
                    /*if(lastBatch != NULL)            *lastBatch = NULL;*/
                    //free((*currentBatch)->ingredient);
                    //free(*currentBatch);

                    //*currentBatch = NULL;
                    //return NULL;
                }else if(lastBatch == NULL) {
                    //cancello testa della lista iterna ma ci sono altri elementi nella lista interna quindi sposto solo la testa
                    temp = currentBatch -> next;
                    //if(currentBatch -> ingredient != NULL)
                    //    free(currentBatch -> ingredient);
                    free(currentBatch);
                    item -> list = temp;
                    //Batch** temp = currentBatch;
                    /*if(lastBatch != NULL)
                        *lastBatch = NULL;*/
                    //free((*currentBatch)->ingredient);
                    //free(*currentBatch);

                    //*currentBatch = (*currentBatch) -> next;
                    //return currentBatch -> next;
                }else {
                    //cancello nodo interno della lista interna
                    //Batch* temp = *currentBatch;
                    lastBatch -> next = currentBatch -> next;
                    temp = currentBatch -> next;
                    //if(currentBatch -> ingredient != NULL)
                     //   free(currentBatch -> ingredient);
                    free(currentBatch);
                    //*lastBatch = *currentBatch;
                    //free(*currentBatch);
                    //*currentBatch = (*currentBatch) -> next;
                    //return (*currentBatch) -> next;
                }
                currentBatch = temp;
                //fixHashTable(table, currentIndex, currentTime, isModified);
                //return;
                //i valori di currentBatch e lastBatch sono stati aggiornati in removeBatchFromHashTable
                //lastBatch non varia in nessun caso
            }else {
                //aggiorno il valore di quantity a quantityLeft
                currentBatch -> quantity = currentBatch -> quantityLeft;
                //lastBatch = currentBatch;
                //currentBatch = currentBatch -> next;
                return;
            }

        }else {
            if(currentBatch -> quantityLeft < currentBatch -> quantity) {
                //ripristino il valore di quantityLeft
                currentBatch -> quantityLeft = currentBatch -> quantity;
                lastBatch = currentBatch;
                currentBatch = currentBatch -> next;
            }else {
                return;
            }
        }
    }
}

int mioArray[50];
int i;

void fixHashTable(HashTable** table, int currentTime, Order* order) {

    Node* currentIngredient = order -> recipe -> ingredientList -> head;

    //ModifiedIndex* currentIndex = modifiedIndexHead;
    //ModifiedIndex* temp;
    //int j = 0;
    //Batch* currentBatch = NULL;
    //Batch* lastBatch = NULL;
    //Batch* nextBatch = NULL;
    while(currentIngredient != NULL) {
        //lastBatch = NULL;
        //currentBatch = (*table) -> items[currentIndex -> index] -> list;

        removeBatchFromHashTable(table, (*currentIngredient -> item), currentTime);

        currentIngredient = currentIngredient -> next;
        //j++;
        //temp = currentIndex -> next;
        //free(currentIndex);
        //currentIndex = temp;
    }
}

int searchBatchInItem(HashTable** table, Item* item, int currentTime, int quantityToFind, bool* isFound) {

    Batch* currentBatch = item -> list;
    //Batch* lastBatch = NULL;
    int indexModified = -1;

    while(currentBatch != NULL) {
        if(currentBatch -> expiration > currentTime) {
            currentBatch -> quantityLeft = currentBatch -> quantity;
            //sistemo quantityLeft in caso fosse stato modificato in precedenza senza che poi siano stati effetivamente usati gli ingredienti
            if(currentBatch -> quantity > 0 && currentBatch -> quantityLeft > 0) {
                indexModified = 1;
                currentBatch -> quantityLeft = currentBatch -> quantity - quantityToFind;
                quantityToFind = quantityToFind - currentBatch -> quantity;
                currentBatch -> lastTimeModified = lastTimeModified;
                if(quantityToFind <= 0) {
                    *isFound = true;
                    return 0;
                }
            }

            //lastBatch = currentBatch;
            currentBatch = currentBatch -> next;
        }else {
            currentBatch -> quantity = 0;
            currentBatch -> quantityLeft = -1;
            currentBatch -> lastTimeModified = lastTimeModified;
            //indexModified = 0;
            //lastBatch = currentBatch;
            currentBatch = currentBatch -> next;
            //devo eliminare il batch scaduto
            //removeBatchFromHashTable(table, &currentBatch, &lastBatch, tryIndex);
            //i valori di currentBatch e lastBatch sono stati aggiornati in removeBatchFromHashTable
            //lastBatch non varia in nessun caso
            //printf("x");
        }
    }
    //restituisco -1 se non ho modificato nessun batch, altrimenti tryIndex se ho modificato almeno un batch
    //e poi però non ho trovato abbastanza quantità di ingrediente
    *isFound = false;
    return indexModified;
}

int searchIngredientInHashTable(HashTable** table, char* ingredientKey, int currentTime, int quantityToFind, bool* isFound) {

    unsigned int index = hashFunction(ingredientKey, (*table)->size);
    //unsigned int step = hashFunction2(ingredientKey, (*table)->size);
    Batch* currentBatch = NULL;
    //Batch* lastBatch = NULL;
    int indexModified = -1;

    for (int i = 0; i < (*table)->size; i++) {
        int tryIndex = (index + i) % (*table)->size;

        if ((*table)->items[tryIndex] == NULL) {
            *isFound = false;
            return -1;
        }

        if (!(*table)->items[tryIndex]->isDeleted && strcmp((*table)->items[tryIndex]->ingredientKey, ingredientKey) == 0) {
            currentBatch = (*table) -> items[tryIndex] -> list;
            //scorro la lista interna e cerco la quantità di ingrediente richiesta
            while(currentBatch != NULL) {
                if(currentBatch -> expiration > currentTime) {
                    currentBatch -> quantityLeft = currentBatch -> quantity;
                    //sistemo quantityLeft in caso fosse stato modificato in precedenza senza che poi siano stati effetivamente usati gli ingredienti
                    if(currentBatch -> quantity > 0 && currentBatch -> quantityLeft > 0) {
                        indexModified = tryIndex;
                        currentBatch -> quantityLeft = currentBatch -> quantity - quantityToFind;
                        quantityToFind = quantityToFind - currentBatch -> quantity;
                        currentBatch -> lastTimeModified = lastTimeModified;
                        if(quantityToFind <= 0) {
                            *isFound = true;
                            return tryIndex;
                        }
                    }

                    //lastBatch = currentBatch;
                    currentBatch = currentBatch -> next;
                }else {
                    currentBatch -> quantity = 0;
                    currentBatch -> quantityLeft = -1;
                    currentBatch -> lastTimeModified = lastTimeModified;
                    indexModified = tryIndex;
                    //lastBatch = currentBatch;
                    currentBatch = currentBatch -> next;
                    //devo eliminare il batch scaduto
                    //removeBatchFromHashTable(table, &currentBatch, &lastBatch, tryIndex);
                    //i valori di currentBatch e lastBatch sono stati aggiornati in removeBatchFromHashTable
                    //lastBatch non varia in nessun caso
                    //printf("x");
                }
            }
            *isFound = false;
            //restituisco -1 se non ho modificato nessun batch, altrimenti tryIndex se ho modificato almeno un batch
            //e poi però non ho trovato abbastanza quantità di ingrediente
            return indexModified;
        }
    }
    *isFound = false;
    //restituisco -1 se non ho modificato nessun batch
    return -1;
}

int searchIngredient(HashTable** table, Order* order, int currentTime) {

    Node* currentIngredient = order -> recipe -> ingredientList -> head;
    int index;
    int quantityOrder = order -> quantity;
    //ModifiedIndex* modifiedIndexHead = NULL;
    bool isFound = true;
    i = 0;

    if((*table) -> items == NULL) {
        lastTimeModified++;
        return -2;              //magazzino vuoto
    }

    while(currentIngredient != NULL) {

        index = searchBatchInItem(table, (*currentIngredient -> item), currentTime, currentIngredient -> quantity * quantityOrder, &isFound);
        //index = searchIngredientInHashTable(table, currentIngredient -> item -> ingredientKey, currentTime, currentIngredient -> quantity * quantityOrder, &isFound);
        if(!isFound) {
            order -> weight = 0;
            lastTimeModified++;

            if(index == -1)
                return -1;               //ingrediente mancante
            else
                return 1;              //ingrediente non abbastanza

        }else {
            order -> weight = order -> weight + (currentIngredient -> quantity * quantityOrder);
            //salvo chiavi hash per poi poterle usare per rimuovere gli ingredienti
            //creo nodo della lista di chiavi hash
            //ModifiedIndex* modifiedIndex = createModifiedIndex(index);
            //inserisco in testa il nuovo index
            //modifiedIndex -> next = modifiedIndexHead;
            //modifiedIndexHead = modifiedIndex;

            //mioArray[i] = index;
            //i++;

            currentIngredient = currentIngredient -> next;
        }
    }
    //pongo isModified a true così che poi posso sistemare gli ingredienti
    //isFound = true;
    //sistemo la lista di ingredienti prima di fare return
    fixHashTable(table, currentTime, order);
    lastTimeModified++;
    return 0;                           //ricetta preparata
}

bool checkIfRecipeIsPresentInReadyQueue(Queue* readyQueue, char* recipeName) {

    Order* currentOrder = readyQueue -> head;
    while (currentOrder != NULL) {
        if(strcmp(currentOrder -> recipe -> name, recipeName) == 0) {
            //ricetta presente nella queue di ordini in attesa
            return true;
        }
        currentOrder = currentOrder -> next;
    }

    return false;
}

void prepareOrder(Queue* waitQueue, Queue* readyQueue, HashTable** listOfLists, int currentTime) {

    //scorro tutta la queue e verifico se ogni ordine è preparabile o no
    //se un ordine è preparabile rimuovo gli ingredienti dai batch e sposto l'ordine nella coda degli ordini pronti
    Order* currentOrder = waitQueue -> head;
    Order* last = NULL;
    Order* next = NULL;
    int result;
    bool flag = true;
    //int condition = currentTime - periodicity;

    while (currentOrder != NULL) {
        //itero sugli ingredienti e controllo con checkIfIngredientIsPresentInWarehouse se sono tutti presenti
        //se sono tutti presenti sposto l'ordine in readyQueue e rimuovo gli ingredienti dal magazzino
        flag = true;

        if(last != NULL) {
            if(currentOrder -> recipe -> state == -1 && currentOrder -> recipe -> lastTimeUpdated == currentTime) {
                last = currentOrder;
                currentOrder = currentOrder -> next;
                flag = false;
            }else if(currentOrder -> recipe -> state <= currentOrder -> quantity && currentOrder -> recipe -> lastTimeUpdated == currentTime) {
                last = currentOrder;
                currentOrder = currentOrder -> next;
                flag = false;
            }
        }


        if(!flag) {
            continue;
        }

        result = searchIngredient(listOfLists, currentOrder, currentTime);
        if(result == 0) {
            //sistemo lista waitQueue
            if(last == NULL){
                waitQueue -> head = currentOrder -> next;
            }else {
                last -> next = currentOrder -> next;
            }
            //devo far prima lo spostamento in avanti se no perdo il riferimento all'ordine next in wait
            next = currentOrder -> next;
            //sposto last che sarebbe l'ordino pronto da wait a ready
            appendOrderInReadyQueue(currentOrder, readyQueue);
            currentOrder = next;
        }else {
            if(result == 1) {
                currentOrder -> recipe -> state = currentOrder -> quantity;
                currentOrder -> recipe -> lastTimeUpdated = currentTime;
            }else if(result == -1) {
                currentOrder -> recipe -> state = -1;
                currentOrder -> recipe -> lastTimeUpdated = currentTime;
            }else{
                //result == -2
                if(last != NULL)
                    waitQueue -> tail = last;
                else
                    waitQueue -> tail = NULL;
                return;
            }
            //se invece non sono tutti presenti non faccio nulla a currentOrder (per farlo conviene creare una nuova coda temporanea se no continuo a iterare all'infinito)
            //appendOrderInWaitQueue(currentOrder, waitQueue);

            last = currentOrder;
            currentOrder = currentOrder -> next;

        }

    }
    if(last != NULL)
        waitQueue -> tail = last;
    else
        waitQueue -> tail = NULL;
    //se almeno un ordine viene processato pongo il magazzino a modificato altrimenti a false
    /*
    if(modifiedNow == true) {
        listOfLists -> modified = true;
    }else {
        listOfLists -> modified = false;
    }
    */
}

bool prepareSingleOrder(HashTable** table, Order* order, int currentTime) {

    if(searchIngredient(table, order, currentTime) == 0)
        return true;
    else
        return false;
}

void removeNewline(char *str) {

    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

void removeRecipeFromHashTable(RecipeHashTable** table, char* recipeName) {

    removeNewline(recipeName);

    unsigned int index = hashFunction(recipeName, (*table)->size);
    //unsigned int step = hashFunction2(recipeName, (*table)->size);

    for (int i = 0; i < (*table) -> size; i++) {
        int tryIndex = (index + i) % (*table)->size;

        if ((*table)->items[tryIndex] == NULL) {
            printf("non presente\n");
            return;
        }

        //controllo se la key della cella è uguale a ingredientKey
        if (strcmp((*table) -> items[tryIndex] -> name, recipeName) == 0) {

            //controllo se la cella è stata cancellata
            if ((*table)->items[tryIndex]->isDeleted) {
                printf("non presente\n");
                return;
            }
            //ricetta già presente nella hashTable
            //controllo se è presente in readyQueue o waitQueue
            if((*table) -> items[tryIndex] -> numberOrder == 0) {
                //la ricetta non è presente in waitQueue quindi posso rimuoverla
                printf("rimossa\n");
                //aggiusto la lista e poi cancello la ricetta
                /*freeIngredientList((*table) -> items[tryIndex] -> ingredientList);
                free((*table) -> items[tryIndex] -> ingredientList);
                free((*table) -> items[tryIndex] -> name);
                free((*table) -> items[tryIndex]);*/
                (*table) -> items[tryIndex] -> isDeleted = true;
                (*table) -> count--;
            }else {
                printf("ordini in sospeso\n");
            }
            return;
        }
    }
    printf("non presente\n");
}

void fillVan(Queue* vanQueue) {

    Order* currentOrder = vanQueue -> head;
    Order* temp = NULL;
    while (currentOrder != NULL) {
        printf("%d %s %d\n", currentOrder -> time, currentOrder -> recipe -> name, currentOrder -> quantity);
        currentOrder -> recipe -> numberOrder--;
        temp = currentOrder -> next;
        //free(currentOrder -> recipeName);
        free(currentOrder);
        currentOrder = temp;
    }
}

void selectOrderToPutInVan(int capacity, Queue* readyQueue) {

    //non ci sono ordini pronti
    if(readyQueue -> head == NULL) {
        printf("camioncino vuoto\n");
        return;
    }

    Queue* vanQueue = createQueue();

    int vanCapacity = capacity;
    Order* currentOrder = readyQueue -> head;
    //Order* last = NULL;
    Order* next = NULL;

    while (currentOrder != NULL && vanCapacity > 0) {

        if(vanCapacity >= currentOrder -> weight) {

            //l'ordine viene caricato sul van
            vanCapacity = vanCapacity - currentOrder -> weight;

            readyQueue -> head = currentOrder -> next;

            //devo far prima lo spostamento in avanti se no perdo il riferimento all'ordine next in wait
            next = currentOrder -> next;
            //sposto last che sarebbe l'ordino pronto da wait a ready
            appendOrderInVanQueue(currentOrder, vanQueue);
            currentOrder = next;

        }else {
            //l'ordine non ci sta sul van, passo a quello successivo
            vanCapacity = 0;
        }
    }
    if(readyQueue -> head == NULL)
        readyQueue -> tail = NULL;

    fillVan(vanQueue);

    free(vanQueue);
}

//
//FUNZIONI UTILS
//

void printHashTable(HashTable* table) {

    for(int i = 0; i < table -> size; i++) {
        if(table -> items[i] != NULL) {
            printf("\nindex: %d  ", i);
            printf("key: %s", table -> items[i] -> ingredientKey);
            Batch* currentBatch = table -> items[i] -> list;
            if(table -> items[i] -> isDeleted == true) {
                printf("_deleted_");
            }
            printf("\n");
            while(currentBatch != NULL) {
                printf("expiration: %d, quantity: %d, quantityLeft: %d\n", currentBatch -> expiration, currentBatch -> quantity, currentBatch -> quantityLeft);
                currentBatch = currentBatch -> next;
            }
        }
    }
}

// Returns the hash of the given string.
int UTILS_hashString(char *command) {
    int hash = 0;
    for (int i = 0; i < strlen(command); i++) {
        hash += command[i];
    }
    return hash;
}


//funzione per leggere input
void UTILS_commandsHandler() {
    char commandBuffer[COMMAND_BUFFER_SIZE];
    char *commandArgumentHolder;
    char *recipeName, *ingredientName;
    int quantity, expiration;
    int tmp;
    Order* order = NULL;
    Batch* batch = NULL;
    int capacity = 0;
    int periodicity = 0;
    int currentTime = 0;
    Recipe* recipe = NULL;
    //size_t length;

    RecipeHashTable* recipeList = createRecipeHashTable();
    HashTable* table = createHashTable();
    Queue* waitQueue = createQueue();
    Queue* readyQueue = createQueue();

    if(fgets(commandBuffer, COMMAND_BUFFER_SIZE, stdin)) {
        commandArgumentHolder = strtok(commandBuffer, COMMAND_ARGUMENTS_DELIMITER);
        periodicity = atoi(commandArgumentHolder);
        commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
        capacity = atoi(commandArgumentHolder);
    }

    while (fgets(commandBuffer, COMMAND_BUFFER_SIZE, stdin)) {
        if(currentTime != 0 && currentTime % periodicity == 0) {
            //arriva il furgone
            //lo riempo in base alla sua capacity prendendo gli ordini da readyQueue
            selectOrderToPutInVan(capacity, readyQueue);
        }
        commandArgumentHolder = strtok(commandBuffer, COMMAND_ARGUMENTS_DELIMITER);
        tmp = UTILS_hashString(commandArgumentHolder);
        //printf("TIME: %d\n", currentTime);
        switch (tmp) {
        case aggiungi_ricetta_HASH:
            //nome ricetta
            commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
            recipeName = commandArgumentHolder;
            //controllo se è già presente una ricetta con lo stesso nome. se è già presente esco dallo switch
            //altrimenti dopo aver inserito la ricetta nella hash table entro nel while qua sotto
            if(checkIfRecipeIsPresentInHashTable(recipeName, &recipeList) == NULL) {
                //creo Recipe
                recipe = createRecipe(recipeName);
                while ((commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER)) != NULL) {
                    //commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
                    ingredientName = strndup(commandArgumentHolder, 257);
                    commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
                    quantity = atoi(commandArgumentHolder);
                    //creo ingrediente
                    Node* nodeIngredient = createNodeIngredient(ingredientName, quantity);
                    //aggiungo incrediente alla lista di ingredienti di recipe
                    insertItemInHashTable(&table, currentTime, nodeIngredient, ingredientName);
                    appendIngredientToRecipe(nodeIngredient, recipe -> ingredientList);
                    free(ingredientName);
                }
                //aggiungo recipe alla lista di recipe
                insertRecipeInHashTable(&recipeList, recipe);
                printf("aggiunta\n");
            }else {
                printf("ignorato\n");
            }
            break;
        case rimuovi_ricetta_HASH:
            commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);

            recipeName = strndup(commandArgumentHolder, 257);

            //recipeName[length - 1] = '\0';
            //rimuovo ricetta da hash table

            removeRecipeFromHashTable(&recipeList, recipeName);
            free(recipeName);

            break;
        case rifornimento_HASH:
            while ((commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER)) != NULL) {
                ingredientName = strndup(commandArgumentHolder, 257);
                //if(commandArgumentHolder[length] == '\0')
                 //   printf("---------COMMANDARGOMENT HA il terminatore VALE %c, %c\n", commandArgumentHolder[length-1], commandArgumentHolder[length]);
                commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
                quantity = atoi(commandArgumentHolder);
                commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
                expiration = atoi(commandArgumentHolder);
                /*length = strlen(commandArgumentHolder);
                if(commandArgumentHolder[length] == '\n' || commandArgumentHolder[length-1] == '\n')
                    printf("---------COMMANDARGOMENT HA il slash a capo VALE %s\n", commandArgumentHolder);*/
                //creo Batch
                batch = createNodeBatch(expiration, quantity);
                //inserisco Batch nel magazzino (hash table)
                insertBatchInHashTable(&table, batch, currentTime, ingredientName);
                free(ingredientName);
                batch = NULL;
                //free(batch->ingredient);
                //free(batch);
                //printHashTable(listOfLists);
                //printf("\n");
            }
            //printHashTable(listOfLists);
            //controllo se gli ordini in attesa possono essere preparati
            prepareOrder(waitQueue, readyQueue, &table, currentTime);
            printf("rifornito\n");
            //lastTimeRifornimento = currentTime;
            break;
        case ordine_HASH:
            commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
            recipeName = strndup(commandArgumentHolder, 257);

            //controllo se esiste ricetta con questo nome. se esisto vado avanti, altrimenti esco dallo switch
            recipe = checkIfRecipeIsPresentInHashTable(recipeName, &recipeList);
            if(recipe != NULL) {
                printf("accettato\n");
                commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
                quantity = atoi(commandArgumentHolder);
                //creo ordine
                order = createOrder(recipe, quantity, currentTime);
                //invio ordine (poi si deve verificare se l'ordine può essere elaborato o no in base alle scorte, lotti in magazzino)
                //controllo subito se l'ordine può essere preparato. in tal caso lo metto direttamente in readyQueue
                if(prepareSingleOrder(&table, order, currentTime)) {
                    //posso preparare subito l'ordine quindi lo metto in readyQueue
                    appendOrderInReadyQueue(order, readyQueue);
                }else {
                    //altrimenti lo metto in waitQueue
                    appendOrderInQueue(order, waitQueue);
                }
            }else {
                printf("rifiutato\n");
            }
            order = NULL;
            free(recipeName);
            break;
        default:
            return;
            //break;
        }
        //printHashTable(table);
        //printf("\n");
        //printHashTable(listOfLists);
        //printf("-------------------------------------------------------------------------------------------");
        currentTime++;
    }
    //ultima volta che arriva il van, dopo l'ultimo comando
    if(currentTime % periodicity == 0) {
        //arriva il furgone. lo riempo in base alla sua capacity prendendo gli ordini da readyQueue
        selectOrderToPutInVan(capacity, readyQueue);
    }
    //printHashTable(table);
    //dealloco le strutture dati
    free(order);
    free(batch);
    freeOrders(readyQueue);
    freeOrders(waitQueue);
    free(readyQueue);
    free(waitQueue);
    //freeRecipeInList(recipeList);
    freeRecipeInHashTable(recipeList);
    free(recipeList);
    freeHashTable(table);
    free(table -> items);
    free(table);
}


int main(void) {

    //printf("Hello, World!\n");

    UTILS_commandsHandler();

    return 0;
}
