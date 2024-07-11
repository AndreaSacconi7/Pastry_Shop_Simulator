#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COMMAND_ARGUMENTS 500

#define MAX_COMMAND_ARGUMENT_LENGTH 255

#define COMMAND_BUFFER_SIZE (MAX_COMMAND_ARGUMENTS * MAX_COMMAND_ARGUMENT_LENGTH)
// The delimiter between arguments is a space.
#define COMMAND_ARGUMENTS_DELIMITER " "

#define aggiungi_ricetta_HASH 1686
#define rimuovi_ricetta_HASH 1622
#define rifornimento_HASH 1308
#define ordine_HASH 641

#define INITIAL_TABLE_SIZE 50
#define LOAD_FACTOR_THRESHOLD 0.7

//ingrediente
typedef struct Node {
    char* ingredientName;
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
    struct Recipe* next;
} Recipe;

typedef struct RecipeList {
    Recipe* head;
} RecipeList;

//lotto (nodo, cella hash table)
typedef struct Batch {
    char* ingredient;
    int expiration;
    int quantity;
    int quantityLeft;
    struct Batch* next;
} Batch;

typedef struct ModifiedIndex {
    int index;
    struct ModifiedIndex* next;
} ModifiedIndex;

// Struttura per il nodo della lista esterna
typedef struct Item {
    char* ingredientKey;
    Batch* list;
    bool isDeleted;
    struct Item* next;
} Item;

//lista di lotti (magazzino)
typedef struct HashTable {
    Item** items;            //lotto con scadenza più vicina (dove estraggo)
    int size;                   //dimensione della tabella hash
    int count;                  //numero di ingredienti presenti
} HashTable;

typedef struct Order {
    int quantity;
    char* recipeName;
    int weight;
    int time;
    List* ingredientList;
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
void removeBatchFromHashTable(HashTable** table, Batch** currentBatch, Batch** lastBatch, int index);


//
//funzioni hash
//

unsigned int hashFunction(char* key, int tableSize) {
    unsigned long int value = 0;
    unsigned int i = 0;
    //unsigned int key_len = strlen(key);

    while (key[i] != '\0') {
        value = (value * 31 + key[i]) % tableSize;
        i++;
    }

    return value;
}

unsigned int hashFunction2(char* key, int tableSize) {
    unsigned long int value = 0;
    unsigned int i = 0;
    //unsigned int key_len = strlen(key);

    while (key[i] != '\0') {
        value = (value * 33 + key[i]) % tableSize;
        i++;
    }

    return value;
}


//
//funzioni create
//

Node* createNodeIngredient(char* nameIngredient, int quantity) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newNode -> ingredientName = strdup(nameIngredient);
    newNode -> quantity = quantity;
    newNode -> next = NULL;
    return newNode;
}

Recipe* createRecipe(char* nameRecipe) {
    Recipe* newRecipe = (Recipe*)malloc(sizeof(Recipe));
    List* ingredientList = (List*)malloc(sizeof(List));
    if (newRecipe == NULL || ingredientList == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newRecipe -> name = strdup(nameRecipe);
    ingredientList -> head = NULL;
    newRecipe -> ingredientList = ingredientList;

    return newRecipe;
}

RecipeList* createRecipeList() {
    RecipeList* newRecipeList = (RecipeList*)malloc(sizeof(RecipeList));
    if(newRecipeList == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newRecipeList -> head = NULL;

    return newRecipeList;
}

Order* createOrder(char* recipeName, int quantity, int currentTime) {

    Order* newOrder = (Order*)malloc(sizeof(Order));
    if(newOrder == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newOrder -> next = NULL;
    newOrder -> recipeName = recipeName;
    newOrder -> quantity = quantity;
    newOrder -> weight = 0;
    newOrder -> time = currentTime;
    newOrder -> ingredientList = NULL;
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

Batch* createNodeBatch(char *ingredient, int expiration, int quantity) {

    Batch* newBatch = (Batch*)malloc(sizeof(Batch));
    if(newBatch == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newBatch -> ingredient = strdup(ingredient);
    newBatch -> expiration = expiration;
    newBatch -> quantity = quantity;
    newBatch -> quantityLeft = quantity;
    newBatch -> next = NULL;

    return newBatch;
}

ModifiedIndex* createModifiedIndex(int index) {
    ModifiedIndex* newIndex = (ModifiedIndex*)malloc(sizeof(ModifiedIndex));
    if(newIndex == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newIndex -> index = index;
    newIndex -> next = NULL;

    return newIndex;
}

Item* createItem(char* key) {
    Item* item = (Item*)malloc(sizeof(Item));
    if (item == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    item -> ingredientKey = strdup(key);
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
        int cmp = strcmp(temp -> ingredientName , newNode -> ingredientName);
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

void appendRecipeToList(Recipe* newRecipe, RecipeList* list) {

    if(list -> head == NULL) {
        list -> head = newRecipe;
        newRecipe -> next = NULL;
    }

    Recipe* temp = list -> head;
    Recipe* last = NULL;

    while(temp != NULL) {
        int cmp = strcmp(temp -> name , newRecipe -> name);
        if(cmp < 0) {
            last = temp;
            temp = temp -> next;

        }else if(cmp == 0){
            //ricetta già presente nella lista
            return;

        }else {
            //posiziono il newNode prima del nodo temp
            if(last != NULL) {
                //inserisco prima di temp e dopo last
                last -> next = newRecipe;
                newRecipe -> next = temp;
            }else {
                //inserisco in testa
                newRecipe -> next = list -> head;
                list -> head = newRecipe;
            }
            return;
        }
    }
    //inserimento in coda
    if(last != NULL) {
        //inserimento in coda con X elementi in lista
        last -> next = newRecipe;
        newRecipe -> next = NULL;
    }
}

/*
void fixSuccesorBatch(Batch* currentBatch, bool modified) {

    while(currentBatch != NULL) {

        if(currentBatch -> quantityLeft < currentBatch -> quantity) {
            if(modified)
                currentBatch -> quantity = currentBatch -> quantityLeft;
            else
                currentBatch -> quantityLeft = currentBatch -> quantity;
        }
        currentBatch = currentBatch -> next;
    }
}
*/
/*
//la internal list viene ordinata in ordine di expiration
//scadenza più vicina -> scadenza più lontana
void appendToInternalList(InternalList* list, Batch* newNode, bool modified) {

    if (list -> head == NULL) {
        list -> head = newNode;
        newNode -> next = NULL;
        return;
    }

    Batch* currentBatch = list -> head;
    Batch* last = NULL;

    while(currentBatch != NULL) {

        if(currentBatch -> expiration <= newNode -> expiration) {
            last = currentBatch;
            currentBatch = currentBatch -> next;
        }else {
            if(last == NULL) {
                newNode -> next = list -> head;
                list -> head = newNode;
            }else {
                last -> next = newNode;
                newNode -> next = currentBatch;
            }
            //aggiusto i successivi batch compreso il currentBatch poichè viene posto dopo newNode
            fixSuccesorBatch(currentBatch, modified);
            return;
        }
    }
    //inserimento in coda dopo aver scorso tutta la lista
    if(last == NULL) {
        //inserimento in coda con un solo elemento (non dovrebbe succedere)
        list -> head -> next = newNode;
        newNode -> next = NULL;
    }else {
        last -> next = newNode;
        newNode -> next = NULL;
    }
}
*/

/*
void appendToListOfLists(Item* lastNode, InternalList* newInternalList, HashTable* listOfLists) {
    Item* newListNode = (Item*)malloc(sizeof(Item));
    if (newListNode == NULL) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newListNode->list = newInternalList;
    newListNode->next = NULL;

    if(lastNode != NULL) {
        //inserisco in mezzo o in coda
        Item* nextNode = lastNode -> next;
        lastNode -> next = newListNode;
        newListNode -> next = nextNode;
    }else {
        //inserisco in testa
        Item* oldHead = listOfLists -> head;
        listOfLists -> head = newListNode;
        newListNode -> next = oldHead;
    }
}
*/

void appendToInternalList(HashTable** table,Batch** head, Batch* newBatch, int currentTime, int index) {

    if (*head == NULL) {
        *head = newBatch;
        newBatch -> next = NULL;
        return;
    }

    Batch* currentBatch = *head;
    Batch* last = NULL;

    while(currentBatch != NULL) {

        if(currentBatch -> expiration <= newBatch -> expiration) {
            last = currentBatch;
            currentBatch = currentBatch -> next;
        }else {
            if(last == NULL) {
                Item* newItem = createItem(newBatch -> ingredient);
                newItem -> list = newBatch;
                newBatch -> next = NULL;
                (*table)->items[index] = newItem;
                newBatch -> next = *head;
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


void insertBatchInHashTable(HashTable** table, Batch* newBatch, int currentTime) {

    //se il batch è scaduto non lo inserisco
    if(newBatch -> expiration <= currentTime)
        return;

    double loadFactor = (double)(*table) -> count / (double)(*table) -> size;
    if(loadFactor > LOAD_FACTOR_THRESHOLD) {
        resize(table, currentTime);
    }
    Item* newItem = NULL;
    char* ingredientKey = newBatch -> ingredient;

    unsigned int index = hashFunction(ingredientKey, (*table)->size);
    unsigned int step = hashFunction2(ingredientKey, (*table)->size);

    for (int i = 0; i < (*table) -> size; i++) {
        int tryIndex = (index + i * step) % (*table)->size;

        if ((*table)->items[tryIndex] == NULL) {
            newItem = createItem(ingredientKey);
            newItem -> list = newBatch;
            newBatch -> next = NULL;
            (*table)->items[tryIndex] = newItem;
            (*table)->count++;
            return;
        }

        //controllo se la key della cella è uguale a ingredientKey
        if (strcmp((*table) -> items[tryIndex] -> ingredientKey, ingredientKey) == 0) {

            //controllo se la cella è stata cancellata
            if ((*table)->items[tryIndex]->isDeleted) {
                //se la cella è stata cancellata devo inserire il nuovo batch nella cella in cima
                newItem = createItem(ingredientKey);
                newItem -> list = newBatch;
                newBatch -> next = NULL;
                (*table)->items[tryIndex] = newItem;
                (*table)->count++;
                return;
            }

            //se la cella non è stata cancellata devo inserire il nuovo batch nella lista interna
            Batch* headBatch = (*table) -> items[tryIndex] -> list;
            //scorro la lista interna e inserisco il nuovo batch in ordine di expiration
            appendToInternalList(table, &headBatch, newBatch, currentTime, index);
            return;
        }
    }
    //per debug
    printf("-------------------------error in insert hash table");
}

void resize(HashTable** table, int currentTime) {

    int newSize = (*table) -> size * 2;
    Batch* newBatch = NULL;
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

            while (batch != NULL) {
                newBatch = createNodeBatch(batch -> ingredient, batch -> expiration, batch -> quantity);
                insertBatchInHashTable(table, newBatch, currentTime);
                batch = batch->next;
            }
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

Recipe* checkIfRecipeIsPresent(char* recipe, RecipeList* list) {

    Recipe* temp = list -> head;

    while (temp != NULL) {
        int cmp = strcmp(temp -> name , recipe);
        if(cmp < 0) {
            temp = temp -> next;

        }else if(cmp == 0){
            //ricetta già presente nella lista
            return temp;

        }else {
            //ho superato l'ordine alfabetico di recipe. quindi non esiste la ricetta recipe
            return NULL;
        }
    }
    return NULL;
}

void removeBatchFromHashTable(HashTable** table, Batch** currentBatch, Batch** lastBatch, int index) {

    if((lastBatch == NULL || *lastBatch == NULL) && (*currentBatch) -> next == NULL) {
        //cancello testa della lista interna senza che ci siano altri elementi quindi pongo isDeleted a true
        (*table) -> items[index] -> isDeleted = true;
        /*(*currentBatch) -> quantity = 0;
        (*currentBatch) -> quantityLeft = 0;*/
        //Batch** temp = currentBatch;
        /*if(lastBatch != NULL)            *lastBatch = NULL;*/
        //free((*currentBatch)->ingredient);
        //free(*currentBatch);

        *currentBatch = NULL;
        //free(*temp);
        //return NULL;
    }else if(lastBatch == NULL || *lastBatch == NULL) {
        //cancello testa della lista iterna ma ci sono altri elementi nella lista interna quindi sposto solo la testa
        (*table) -> items[index] -> list = (*currentBatch) -> next;
        //Batch** temp = currentBatch;
        /*if(lastBatch != NULL)
            *lastBatch = NULL;*/
        //free((*currentBatch)->ingredient);
        //free(*currentBatch);

        *currentBatch = (*currentBatch) -> next;
        //free(*temp);
        //return (*currentBatch) -> next;
    }else {
        //cancello nodo interno della lista interna
        //Batch* temp = *currentBatch;
        (*lastBatch) -> next = (*currentBatch) -> next;
        //*lastBatch = *currentBatch;
        //free(*currentBatch);
        *currentBatch = (*currentBatch) -> next;
        //free(temp);
        //return (*currentBatch) -> next;
    }
}

void fixHashTable(HashTable** table, ModifiedIndex* modifiedIndexHead, int currentTime, bool isModified) {

    ModifiedIndex* currentIndex = modifiedIndexHead;
    Batch* currentBatch = NULL;
    Batch* lastBatch = NULL;
    while(currentIndex != NULL) {
        lastBatch = NULL;
        currentBatch = (*table) -> items[currentIndex -> index] -> list;
        while(currentBatch != NULL) {
            //se ingrediente scaduto lo pongo a 0 così che poi lo elimino sotto
            /*if(currentBatch -> expiration <= currentTime) {
                currentBatch -> quantityLeft = -1;
            }*/
            if(isModified == true || currentBatch -> expiration <= currentTime) {

                if(currentBatch -> quantityLeft < currentBatch -> quantity) {
                    if(currentBatch -> quantityLeft <= 0) {
                        currentBatch -> quantity = currentBatch -> quantityLeft;
                        //devo cancellare il batch
                        removeBatchFromHashTable(table, &currentBatch, &lastBatch, currentIndex -> index);
                        //i valori di currentBatch e lastBatch sono stati aggiornati in removeBatchFromHashTable
                        //lastBatch non varia in nessun caso
                    }else {
                        //aggiorno il valore di quantity a quantityLeft
                        currentBatch -> quantity = currentBatch -> quantityLeft;
                        lastBatch = currentBatch;
                        currentBatch = currentBatch -> next;
                    }
                }else {
                    lastBatch = currentBatch;
                    currentBatch = currentBatch -> next;
                }
            }else {
                //ripristino il valore di quantityLeft
                currentBatch -> quantityLeft = currentBatch -> quantity;
                lastBatch = currentBatch;
                currentBatch = currentBatch -> next;
            }
        }
        currentIndex = currentIndex -> next;
    }
}

int searchIngredientInHashTable(HashTable** table, char* ingredientKey, int currentTime, int quantityToFind, bool* isFound) {

    unsigned int index = hashFunction(ingredientKey, (*table)->size);
    unsigned int step = hashFunction2(ingredientKey, (*table)->size);
    Batch* currentBatch = NULL;
    Batch* lastBatch = NULL;
    int indexModified = -1;

    for (int i = 0; i < (*table)->size; i++) {
        int tryIndex = (index + i * step) % (*table)->size;

        if ((*table)->items[tryIndex] == NULL) {
            *isFound = false;
            return -1;
        }

        if (!(*table)->items[tryIndex]->isDeleted && strcmp((*table)->items[tryIndex]->ingredientKey, ingredientKey) == 0) {
            currentBatch = (*table) -> items[tryIndex] -> list;
            //scorro la lista interna e cerco la quantità di ingrediente richiesta
            while(currentBatch != NULL) {
                if(currentBatch -> expiration > currentTime) {
                    //sistemo quantityLeft in caso fosse stato modificato in precedenza senza che poi siano stati effetivamente usati gli ingredienti
                    if(currentBatch -> quantity > 0) {
                        indexModified = tryIndex;
                        currentBatch -> quantityLeft = currentBatch -> quantity - quantityToFind;
                        quantityToFind = quantityToFind - currentBatch -> quantity;
                        if(quantityToFind <= 0) {
                            *isFound = true;
                            return tryIndex;
                        }
                    }

                    lastBatch = currentBatch;
                    currentBatch = currentBatch -> next;
                }else {
                    currentBatch -> quantity = 0;
                    currentBatch -> quantityLeft = -1;
                    indexModified = tryIndex;
                    lastBatch = currentBatch;
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


bool searchIngredient(HashTable** table, Order* order, int currentTime) {

    Node* currentIngredient = order -> ingredientList -> head;
    int index;
    int quantityOrder = order -> quantity;
    ModifiedIndex* modifiedIndexHead = NULL;
    bool isFound = true;

    if((*table) -> items == NULL)
        return false;

    while(currentIngredient != NULL) {

        index = searchIngredientInHashTable(table, currentIngredient -> ingredientName, currentTime, currentIngredient -> quantity * quantityOrder, &isFound);
        if(!isFound) {
            //inserisco ultimo index modificato in testa alla lista di chiavi hash se è != -1
            if(index != -1) {
                //creo nodo della lista di chiavi hash
                ModifiedIndex* modifiedIndex = createModifiedIndex(index);
                //inserisco in testa il nuovo index
                modifiedIndex -> next = modifiedIndexHead;
                modifiedIndexHead = modifiedIndex;
            }
            //sistemo la lista di ingredienti prima di fare return
            fixHashTable(table, modifiedIndexHead, currentTime, false);
            order -> weight = 0;
            return false;
        }else {
            order -> weight = order -> weight + (currentIngredient -> quantity * quantityOrder);
            //salvo chiavi hash per poi poterle usare per rimuovere gli ingredienti
            //creo nodo della lista di chiavi hash
            ModifiedIndex* modifiedIndex = createModifiedIndex(index);
            //inserisco in testa il nuovo index
            modifiedIndex -> next = modifiedIndexHead;
            modifiedIndexHead = modifiedIndex;
            currentIngredient = currentIngredient -> next;
        }
    }
    //pongo isModified a true così che poi posso sistemare gli ingredienti
    //isFound = true;
    //sistemo la lista di ingredienti prima di fare return
    fixHashTable(table, modifiedIndexHead, currentTime, true);
    return true;
}

/*
//restituisco 0 se non c'è nulla da sistemare
//restituisco 1 se ho sistemato quantity
//restituisco 2 se cancello batch e cancello listNode (testa di listOfLists)
//restituisco 3 se cancello batch e cancello listNode (non in testa di listOfLists)
//restituisco 4 se cancello batch
int fixBatchWareHouse(Batch* currentBatch, Batch* lastBatch, Item* currentNode, Item* lastNode, HashTable* wareHouseListofLists) {

    //se nella chiamata del metodo precedente ho tolto (virtualmente)
    if(currentBatch -> quantityLeft < currentBatch -> quantity) {
        if(currentBatch -> quantityLeft <= 0) {
            Batch* temp = NULL;
            //cancello nodo
            if(lastBatch == NULL) {
                if(currentBatch -> next == NULL){
                    //devo cancellare il nodo in testa (non ci sono altri batch)
                    //oltre cancellare il nodo in testa cancello anche il currentNode perchè la sua lista ormai sarà vuota
                    temp = currentBatch;
                    Item* tempNode = NULL;
                    //controllo se siamo in cima alla lista di liste
                    if(lastNode == NULL) {
                        //controllo se ci sono altri nodi oltre la testa
                        if(currentNode -> next != NULL) {

                            wareHouseListofLists -> head = currentNode -> next;
                            return 5;

                        }else {
                            //sono in cima alla lista di liste
                            wareHouseListofLists -> head = NULL;
                            tempNode = currentNode;
                            free(tempNode);
                            //currentNode = NULL;
                            free(temp);
                            //currentBatch = NULL;
                            return 2;
                        }

                    }else {
                        //non sono in cima alla lista di liste
                        tempNode = currentNode;
                        lastNode -> next = currentNode -> next;
                        free(tempNode);
                        //currentNode = lastNode;
                        free(temp);
                        //currentBatch = NULL;
                        return 3;
                    }
                }else {
                    //cancello batch in testa ma ci sono altri elementi nella internal List
                    //temp = currentBatch;
                    currentNode -> list -> head = currentBatch -> next;
                    //free(temp);
                    return 6;
                }
            }else {
                //temp = currentBatch;
                lastBatch -> next = currentBatch -> next;
                //currentBatch = currentBatch -> next;
                return 4;
            }
        }else {
            //modifico quantity poichè la quantità è diminuita ma non è finita
            currentBatch -> quantity = currentBatch -> quantityLeft;
            return 1;
        }
    }
    return 0;
}

bool checkIfIngredientIsPresentInNotModifiedWareHouse(HashTable* wareHouseListofLists, Order* order, int currentTime) {

    Item* currentNode = wareHouseListofLists -> head;
    Item* lastNode = NULL;
    Node* currentIngredient = order -> ingredientList -> head;
    int quantityOrder = order -> quantity;
    bool ingredientFound = false;
    Batch* lastBatch = NULL;

    while (currentNode != NULL && currentIngredient != NULL) {
        int cmp = strcmp(currentNode -> list -> head -> ingredient, currentIngredient -> ingredientName);
        if(cmp == 0) {
            lastBatch = NULL;
            Batch* currentBatch = currentNode -> list -> head;
            //se per preparare 1 torta mi servono 2 uova. per prepararne X mi servono 2*X uova
            int quantityToFind = currentIngredient -> quantity * quantityOrder;
            while(!ingredientFound && currentBatch != NULL) {

                //se avevo modificato quantityLeft ma poi non ho realmente tolto gli ingredienti dal magazzino rispristino il valore iniziale
                currentBatch -> quantityLeft = currentBatch -> quantity;
                if(currentBatch -> expiration > currentTime) {
                    currentBatch -> quantityLeft = currentBatch -> quantity - quantityToFind;
                    quantityToFind = quantityToFind - currentBatch -> quantity;
                    if(quantityToFind <= 0)
                        ingredientFound = true;

                    lastBatch = currentBatch;
                    currentBatch = currentBatch -> next;
                }else {
                    //cancello nodo scaduto in base alla casistica
                    currentBatch -> quantityLeft = 0;
                    //chiamo metodo fix per cancellare il batch e eventualemnte il currentNode se rimane vuoto
                    int result = fixBatchWareHouse(currentBatch, lastBatch, currentNode, lastNode, wareHouseListofLists);
                    switch (result) {
                    case 2:
                        lastNode = NULL;
                        currentNode = NULL;
                        currentBatch = NULL;
                        lastBatch = NULL;
                        break;
                    case 3:
                        currentNode = lastNode;
                        currentBatch = NULL;
                        lastBatch = NULL;
                        break;
                    case 4:
                        currentBatch = currentBatch -> next;
                        break;
                    case 5:
                        free(currentNode);
                        free(currentBatch);
                        return checkIfIngredientIsPresentInNotModifiedWareHouse(wareHouseListofLists, order, currentTime);
                    case 6:
                        currentBatch = currentBatch -> next;
                        break;
                    default:
                        //return 0 oppure 1
                        //printf("error in fix");
                        break;
                    }
                }
            }

            if(ingredientFound){
                //sommo i pesi degli ingredienti
                order -> weight = order -> weight + (currentIngredient -> quantity * quantityOrder);
                currentIngredient = currentIngredient -> next;
                ingredientFound = false;
            }else {
                order -> weight = 0;
                return false;
            }

        }else if(cmp > 0){
            //non è presente alcun lotto del ingrediente cercato
            order -> weight = 0;
            return false;
        }
        lastNode = currentNode;
        if(currentNode != NULL)
            currentNode = currentNode -> next;
    }
    //tutti gli ingredienti trovati
    //prima di restituire true devo cancellare tutti i batch che utilizzo per preparare l'ordine
    if(currentIngredient == NULL) {
        wareHouseListofLists -> modified = true;
        return true;
    }else {
        order -> weight = 0;
        return false;
    }
}

bool checkIfIngredientIsPresentInModifiedWareHouse(HashTable* wareHouseListofLists, Order* order, int currentTime, int numberIngredientToFind, int numberIngredientFound) {

    Item* currentNode = wareHouseListofLists -> head;
    Item* lastNode = NULL;
    Node* currentIngredient = order -> ingredientList -> head;
    int quantityOrder = order -> quantity;
    bool ingredientFound = false;
    Batch* lastBatch = NULL;
    int result;

    while (currentNode != NULL && currentIngredient != NULL) {

        int cmp = strcmp(currentNode -> list -> head -> ingredient, currentIngredient -> ingredientName);
        Batch* currentBatch = currentNode -> list -> head;
        if(cmp == 0) {
            lastBatch = NULL;
            //se per preparare 1 torta mi servono 2 uova. per prepararne X mi servono 2*X uova
            int quantityToFind = currentIngredient -> quantity * quantityOrder;
            while(!ingredientFound && currentBatch != NULL) {

                if(currentBatch -> expiration <= currentTime) {
                    currentBatch -> quantityLeft = 0;
                }
                result = fixBatchWareHouse(currentBatch, lastBatch, currentNode, lastNode, wareHouseListofLists);
                switch (result) {
                    case 2:
                        lastNode = NULL;
                        currentNode = NULL;
                        currentBatch = NULL;
                        lastBatch = NULL;
                        break;
                    case 3:
                        currentNode = lastNode;
                        currentBatch = NULL;
                        lastBatch = NULL;
                        break;
                    case 4:
                        currentBatch = currentBatch -> next;
                        break;
                    case 5:
                        free(currentNode);
                        free(currentBatch);
                        return checkIfIngredientIsPresentInModifiedWareHouse(wareHouseListofLists, order, currentTime, numberIngredientToFind, numberIngredientFound);
                    case 6:
                        currentBatch = currentBatch -> next;
                        break;
                    default:
                        //return 0 oppure 1 (currentBatch ancora disponibile)
                        currentBatch -> quantityLeft = currentBatch -> quantity;
                        currentBatch -> quantityLeft = currentBatch -> quantity - quantityToFind;
                        quantityToFind = quantityToFind - currentBatch -> quantity;
                        if(quantityToFind <= 0)
                            ingredientFound = true;

                        lastBatch = currentBatch;
                        currentBatch = currentBatch -> next;
                        break;
                }
            }

            if(ingredientFound){
                //sommo i pesi degli ingredienti
                numberIngredientFound++;
                order -> weight = order -> weight + (currentIngredient -> quantity * quantityOrder);
                currentIngredient = currentIngredient -> next;
                if(currentIngredient != NULL)
                    numberIngredientToFind++;
                ingredientFound = false;
            }else {
                order -> weight = 0;
                //orderPrepared = false;
            }

        }else{
            lastBatch = NULL;
            while(currentBatch != NULL) {
                //cancello nodo scaduto in base alla casistica
                if(currentBatch -> expiration <= currentTime)
                    currentBatch -> quantityLeft = 0;
                //chiamo metodo fix per cancellare il batch e eventualemnte il currentNode se rimane vuoto
                result = fixBatchWareHouse(currentBatch, lastBatch, currentNode, lastNode, wareHouseListofLists);
                switch (result) {
                    case 2:
                        lastNode = NULL;
                        currentNode = NULL;
                        currentBatch = NULL;
                        lastBatch = NULL;
                        break;
                    case 3:
                        currentNode = lastNode;
                        currentBatch = NULL;
                        lastBatch = NULL;
                        break;
                    case 4:
                        currentBatch = currentBatch -> next;
                        break;
                    case 5:
                        free(currentNode);
                        free(currentBatch);
                        return checkIfIngredientIsPresentInModifiedWareHouse(wareHouseListofLists, order, currentTime, numberIngredientToFind, numberIngredientFound);
                    case 6:
                        currentBatch = currentBatch -> next;
                        break;
                    default:
                        //return 0 oppure 1
                        //printf("error in fix");
                        break;
                }
                lastBatch = currentBatch;
                if(currentBatch != NULL)
                    currentBatch = currentBatch -> next;
            }
        }
        lastNode = currentNode;
        if(currentNode != NULL) {
            currentNode = currentNode -> next;
        }
    }
    //in base a orderPrepared capisco se ho trovato tutti gli ingredienti oppure no
    //prima di restituire true devo cancellare tutti i batch che utilizzo per preparare l'ordine
    if(currentIngredient == NULL && numberIngredientFound == numberIngredientToFind) {
        wareHouseListofLists -> modified = true;
        return true;
    }else {
        wareHouseListofLists -> modified = false;
        order -> weight = 0;
        return false;
    }
}

bool checkIfIngredientIsPresentInWareHouse(HashTable* wareHouseListofLists, Order* order, int currentTime) {

    if(wareHouseListofLists -> modified == true) {
        return checkIfIngredientIsPresentInModifiedWareHouse(wareHouseListofLists, order, currentTime, 1, 0);
    }else {
        return checkIfIngredientIsPresentInNotModifiedWareHouse(wareHouseListofLists, order, currentTime);
    }
}
*/
bool checkIfRecipeIsPresentInReadyQueue(Queue* readyQueue, char* recipeName) {

    Order* currentOrder = readyQueue -> head;
    while (currentOrder != NULL) {
        if(strcmp(currentOrder -> recipeName, recipeName) == 0) {
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
    //bool modifiedNow = false;

    while (currentOrder != NULL) {
        //itero sugli ingredienti e controllo con checkIfIngredientIsPresentInWarehouse se sono tutti presenti
        //se sono tutti presenti sposto l'ordine in readyQueue e rimuovo gli ingredienti dal magazzino
        if(searchIngredient(listOfLists, currentOrder, currentTime)) {
            //modifiedNow = true;
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

    return searchIngredient(table, order, currentTime);
}

void removeNewline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

void removeRecipeFromList(char* recipeName, RecipeList* recipeList, Queue* readyQueue, Queue* waitQueue) {

    if(recipeList -> head == NULL) {
        printf("non presente\n");
        return;
    }

    removeNewline(recipeName);

    Recipe* currentRecipe = recipeList -> head;
    Recipe* last = NULL;

    while (currentRecipe != NULL) {
        int cmp = strcmp(currentRecipe -> name , recipeName);
        if(cmp < 0) {
            last = currentRecipe;
            currentRecipe = currentRecipe -> next;

        }else if(cmp == 0){
            //ricetta già presente nella lista
            //controllo se ci sono ordini in sospeso relativi a questa ricetta.
            //se si stampo in sospeso e non faccio nulla altrimenti rimossa e la rimuovo
            if(!checkIfRecipeIsPresentInReadyQueue(readyQueue, recipeName) && !checkIfRecipeIsPresentInReadyQueue(waitQueue, recipeName)) {
                //la ricetta non è presente in waitQueue quindi posso rimuoverla
                printf("rimossa\n");
                //aggiusto la lista e poi cancello la ricetta
                if(last != NULL) {
                    last -> next = currentRecipe -> next;
                }else {
                    recipeList -> head = currentRecipe -> next;
                }
                free(currentRecipe);
            }else {
                printf("ordini in sospeso\n");
            }
            return;

        }else {
            //ho superato l'ordine alfabetico di recipeName. quindi non è presente nella lista
            printf("non presente\n");
            return;
        }
    }

    printf("non presente\n");
}

void fillVan(Queue* vanQueue) {

    Order* currentOrder = vanQueue -> head;
    Order* temp = NULL;
    while (currentOrder != NULL) {
        printf("%d %s %d\n", currentOrder -> time, currentOrder -> recipeName, currentOrder -> quantity);
        temp = currentOrder;
        currentOrder = currentOrder -> next;
        free(temp);
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
            printf("index: %d  ", i);
            Batch* currentBatch = table -> items[i] -> list;
            if(table -> items[i] -> isDeleted == true) {
                printf("_deleted_");
            }
            printf("\n");
            while(currentBatch != NULL) {
                printf("ingredient: %s, expiration: %d, quantity: %d, quantityLeft: %d\n", currentBatch -> ingredient, currentBatch -> expiration, currentBatch -> quantity, currentBatch -> quantityLeft);
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
    int periodicity = 0;
    int capacity = 0;
    int currentTime = 0;
    Recipe* recipe = NULL;

    RecipeList* recipeList = createRecipeList();
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
            if(checkIfRecipeIsPresent(recipeName, recipeList) == NULL) {
                //creo Recipe
                recipe = createRecipe(recipeName);
                while ((commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER)) != NULL) {
                    //commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
                    ingredientName = strdup(commandArgumentHolder);
                    commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
                    quantity = atoi(commandArgumentHolder);
                    //creo ingrediente
                    Node* nodeIngredient = createNodeIngredient(ingredientName, quantity);
                    //aggiungo incrediente alla lista di ingredienti di recipe
                    appendIngredientToRecipe(nodeIngredient, recipe -> ingredientList);
                }
                //aggiungo recipe alla lista di recipe
                appendRecipeToList(recipe, recipeList);
                printf("aggiunta\n");
            }else {
                printf("ignorato\n");
            }
            break;
        case rimuovi_ricetta_HASH:
            commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
            recipeName = strdup(commandArgumentHolder);
            //rimuovo ricetta da hash table
            removeRecipeFromList(recipeName, recipeList, readyQueue, waitQueue);
            break;
        case rifornimento_HASH:
            while ((commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER)) != NULL) {
                ingredientName = strdup(commandArgumentHolder);
                commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
                quantity = atoi(commandArgumentHolder);
                commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
                expiration = atoi(commandArgumentHolder);
                //creo Batch
                Batch* batch = createNodeBatch(ingredientName, expiration, quantity);
                //inserisco Batch nel magazzino (hash table)
                insertBatchInHashTable(&table, batch, currentTime);
                //printHashTable(listOfLists);
                //printf("\n");
            }
            //printHashTable(listOfLists);
            //controllo se gli ordini in attesa possono essere preparati
            prepareOrder(waitQueue, readyQueue, &table, currentTime);
            printf("rifornito\n");
            break;
        case ordine_HASH:
            commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
            recipeName = strdup(commandArgumentHolder);
            //controllo se esiste ricetta con questo nome. se esisto vado avanti, altrimenti esco dallo switch
            recipe = checkIfRecipeIsPresent(recipeName, recipeList);
            if(recipe != NULL) {
                printf("accettato\n");
                commandArgumentHolder = strtok(NULL, COMMAND_ARGUMENTS_DELIMITER);
                quantity = atoi(commandArgumentHolder);
                //creo ordine
                order = createOrder(recipeName, quantity, currentTime);
                order -> ingredientList = recipe -> ingredientList;
                //invio ordine (poi si deve verificare se l'ordine può essere elaborato o no in base alle scorte, lotti in magazzino)
                //controllo subito se l'ordine può essere preparato. in tal caso lo metto direttamente in readyQueue
                if(prepareSingleOrder(&table, order, currentTime)) {
                    //posso preparare subito l'ordine quindi lo metto in readyQueue
                    //printf("-----------preparo ordine %s-----------\n", order ->recipeName);
                    appendOrderInReadyQueue(order, readyQueue);
                }else {
                    //printf("-----------metto in attesa %s----------\n", order -> recipeName);
                    //altrimenti lo metto in waitQueue
                    appendOrderInQueue(order, waitQueue);
                }
            }else {
                printf("rifiutato\n");
            }
            break;
        default:
            return;
            //break;
        }
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
    printHashTable(table);
}


int main(void) {

    //printf("Hello, World!\n");

    UTILS_commandsHandler();

    return 0;
}

/*
         if(ingredientFound){
             ingredientFound = false;
             Batch* headBatchList = currentNode -> list -> head;
             Batch* temp = NULL;
             //ciclo fino alla fine dei lotti o fino a che i lotti sono scaduti
             while (headBatchList != NULL && headBatchList -> expiration > currentTime) {
                 if(headBatchList == currentBatch)
                     break;
                 temp = headBatchList;
                 headBatchList = temp -> next;
                 free(temp);
             }
             if(quantityBatchLeft == 0) {
                 //ultimo lotto svuotato. lo elimino
                 temp = currentBatch;
                 currentBatch = currentBatch -> next;
                 currentNode -> list -> head = currentBatch;
                 free(temp);
             }else {
                 //ultimo lotto va diminuita solo la quantitò
                 currentBatch -> quantity = quantityBatchLeft;
                 currentNode -> list -> head = currentBatch;
             }

         }else
             return 1;
         */