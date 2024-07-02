#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_COMMAND_ARGUMENTS 500

#define MAX_COMMAND_ARGUMENT_LENGTH 255

#define COMMAND_BUFFER_SIZE MAX_COMMAND_ARGUMENTS * MAX_COMMAND_ARGUMENT_LENGTH
// The delimiter between arguments is a space.
#define COMMAND_ARGUMENTS_DELIMITER " "

#define aggiungi_ricetta_HASH 1686
#define rimuovi_ricetta_HASH 1622
#define rifornimento_HASH 1308
#define ordine_HASH 641



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

typedef struct UsedBatch {
    Batch* batch;
    struct UsedBatch* next;
    struct UsedBatch* previous;
} UsedBatch;

typedef struct UsedBatchList {
    UsedBatch* head;
} UsedBatchList;

// Struttura per la lista interna con puntatori a head e tail
typedef struct InternalList {
    Batch* head;
} InternalList;

// Struttura per il nodo della lista esterna
typedef struct ListNode {
    InternalList* list;
    struct ListNode* next;
} ListNode;

//lista di lotti (magazzino)
typedef struct ListOfList {
    ListNode* head;            //lotto con scadenza più vicina (dove estraggo)
    bool modified;
} ListOfList;

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

Node* createNodeIngredient(char* nameIngredient, int quantity) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) {
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
    if (!newRecipe || !ingredientList) {
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
    if(!newRecipeList) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newRecipeList -> head = NULL;

    return newRecipeList;
}

Order* createOrder(char* recipeName, int quantity, int currentTime) {

    Order* newOrder = (Order*)malloc(sizeof(Order));
    if(!newOrder) {
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
    if(!newQueue) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newQueue -> head = NULL;
    newQueue -> tail = NULL;
    return newQueue;
}

UsedBatch* createUsedBatch(Batch* batch) {
    UsedBatch* newUsedBatch = (UsedBatch*)malloc(sizeof(UsedBatch));
    if(!newUsedBatch) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newUsedBatch -> batch = batch;
    newUsedBatch -> next = NULL;
    newUsedBatch -> previous = NULL;
}

UsedBatchList* createUsedBatchList() {
    UsedBatchList* usedBatchList = (UsedBatchList*)malloc(sizeof(UsedBatchList));
    if(!usedBatchList) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    usedBatchList -> head = NULL;
}

void appendUsedBatchToUsedList(UsedBatch* usedBatch, UsedBatchList* usedBatchList) {

    //inserisco in testa
    UsedBatch* temp = usedBatchList -> head;
    usedBatchList -> head = usedBatch;
    usedBatch -> next = temp;
}

void appendIngredientToList(Node* newNode, List* list){

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
}

Batch* createNodeBatch(char *ingredient, int expiration, int quantity) {

    Batch* newBatch = (Batch*)malloc(sizeof(Batch));
    if(!newBatch) {
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

InternalList* createInternalList() {
    InternalList* newList = (InternalList*)malloc(sizeof(InternalList));
    if (!newList) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newList->head = NULL;
    return newList;
}

ListOfList* createListOfLists() {
    ListOfList* newListOfLists = (ListOfList*)malloc(sizeof(ListOfList));
    if (!newListOfLists) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newListOfLists -> head = NULL;
    newListOfLists -> modified = false;
    return newListOfLists;
}

//la internal list viene ordinata in ordine di expiration
//scadenza più vicina -> scadenza più lontana
void appendToInternalList(InternalList* list, Batch* newNode) {

    if (list->head == NULL) {
        list->head = newNode;
    }

    Batch* currentBatch = list -> head;
    Batch* last = NULL;

    while(currentBatch != NULL) {
        if(currentBatch -> expiration <= newNode -> expiration) {
            last = currentBatch;
            currentBatch = currentBatch -> next;
        }else {
            if(last == NULL) {
                list -> head = newNode;
            }else {
                last -> next = newNode;
                newNode -> next = currentBatch;
            }
            return;
        }
    }
    //inserimento in coda dopo aver scorso tutta la lista
    if(last == NULL) {
        list -> head = newNode;
    }else {
        last -> next = newNode;
        newNode -> next = currentBatch;
    }
}

void appendToListOfLists(ListNode* lastNode, InternalList* newInternalList, ListOfList* listOfLists) {
    ListNode* newListNode = (ListNode*)malloc(sizeof(ListNode));
    if (!newListNode) {
        printf("Errore di allocazione della memoria\n");
        exit(1);
    }
    newListNode->list = newInternalList;
    newListNode->next = NULL;

    if(lastNode != NULL) {
        //inserisco in mezzo o in coda
        ListNode* nextNode = lastNode -> next;
        lastNode -> next = newListNode;
        newListNode -> next = nextNode;
    }else {
        //inserisco in testa
        ListNode* oldHead = listOfLists -> head;
        listOfLists -> head = newListNode;
        newListNode -> next = oldHead;
    }
}

void insertBatchInWareHouse(ListOfList* listOfLists, Batch* newBatch) {

    ListNode* temp = listOfLists -> head;
    ListNode* last = NULL;

    if(listOfLists -> head == NULL) {
        InternalList* newInternalList = createInternalList();
        appendToInternalList(newInternalList, newBatch);
        appendToListOfLists(last, newInternalList, listOfLists);
    }

    while (temp != NULL) {
        int cmp = strcmp(temp -> list -> head -> ingredient, newBatch -> ingredient);
        if(cmp < 0) {
            last = temp;
            temp = temp -> next;

        }else if(cmp == 0){
            //aggiungo in lista interna
            appendToInternalList(temp -> list, newBatch);
            return;

        }else {
            //superato ordine alfabetico di newBatch
            InternalList* newInternalList = createInternalList();
            appendToInternalList(newInternalList, newBatch);
            appendToListOfLists(last, newInternalList, listOfLists);
            return;
        }
    }
    //creo lista interna dopo il nodo last
    InternalList* newInternalList = createInternalList();
    appendToInternalList(newInternalList, newBatch);
    appendToListOfLists(last, newInternalList, listOfLists);
}

void appendOrderInWaitQueue(Order* newOrder, Queue* waitQueue) {

    if(waitQueue -> head == NULL) {
        waitQueue -> tail = newOrder;
        waitQueue -> head = waitQueue -> tail;
        return;
    }

    Order* lastTail = waitQueue -> tail;
    lastTail -> next = newOrder;
    waitQueue -> tail = newOrder;
}

void appendOrderInReadyQueue(Order* newOrder, Queue* readyQueue) {

    if(readyQueue -> head == NULL) {
        readyQueue -> tail = newOrder;
        readyQueue -> head = readyQueue -> tail;
        return;
    }

    Order* currentOrder = readyQueue -> head;
    Order* last = NULL;
    int weight = newOrder -> weight;

    while (currentOrder != NULL) {
        if(currentOrder -> weight < weight) {
            if(last == NULL) {
                readyQueue -> head = newOrder;
                newOrder -> next = currentOrder;
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

    if(last == NULL) {
        readyQueue -> head = newOrder;
        newOrder -> next = currentOrder;
    }else {
        last -> next = newOrder;
        newOrder -> next = NULL;
    }
}

void removeOrderInQueue(Queue* queue) {

    if(queue -> head != NULL) {
        Order* head = queue -> head;
        queue -> head = head -> next;
        free(head);
    }
}

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

bool checkIfIngredientIsPresentInNotModifiedWareHouse(ListOfList* wareHouseListofLists, Order* order, int currentTime) {

    ListNode* currentNode = wareHouseListofLists -> head;
    Node* currentIngredient = order -> ingredientList -> head;
    int quantityOrder = order -> quantity;
    bool ingredientFound = false;
    Batch* last = NULL;

    while (currentNode != NULL && currentIngredient != NULL) {
        int cmp = strcmp(currentNode -> list -> head -> ingredient, currentIngredient -> ingredientName);
        if(cmp == 0) {
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

                    last = currentBatch;
                    currentBatch = currentBatch -> next;
                }else {
                    //cancello nodo scaduto
                    Batch* temp = NULL;
                    if(last == NULL) {
                        temp = currentBatch;
                        currentNode -> list -> head = currentBatch -> next;
                        currentBatch = currentBatch -> next;
                        free(temp);
                    }else {
                        temp = currentBatch;
                        last -> next = currentBatch -> next;
                        currentBatch = currentBatch -> next;
                        free(temp);
                    }
                }
            }

            if(ingredientFound){
                //sommo i pesi degli ingredienti
                order -> weight = order -> weight + currentIngredient -> quantity;
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
        currentNode = currentNode -> next;
    }
    //tutti gli ingredienti trovati
    //prima di restituire true devo cancellare tutti i batch che utilizzo per preparare l'ordine
    wareHouseListofLists -> modified = true;
    return true;
}

bool fixBatchWareHouse(Batch* currentBatch, Batch* last, ListNode* currentNode) {

    //se nella chiamata del metodo precedente ho tolto (virtualmente)
    if(currentBatch -> quantityLeft < currentBatch -> quantity) {
        if(currentBatch -> quantityLeft <= 0) {
            Batch* temp = NULL;
            //cancello nodo
            if(last == NULL) {
                temp = currentBatch;
                currentNode -> list -> head = currentBatch -> next;
                free(temp);
            }else {
                temp = currentBatch;
                last -> next = currentBatch -> next;
                currentBatch = currentBatch -> next;
                free(temp);
            }
        }else {
            //modifico quantity poichè la quantità è diminuita ma non è finita
            currentBatch -> quantity = currentBatch -> quantityLeft;
        }
        return true;
    }
    return false;
}

bool checkIfIngredientIsPresentInModifiedWareHouse(ListOfList* wareHouseListofLists, Order* order, int currentTime) {

    ListNode* currentNode = wareHouseListofLists -> head;
    Node* currentIngredient = order -> ingredientList -> head;
    int quantityOrder = order -> quantity;
    bool ingredientFound = false;
    Batch* last = NULL;
    bool orderPrepared = true;

    while (currentNode != NULL && currentIngredient != NULL) {
        if(currentNode -> list -> head != NULL){
            int cmp = strcmp(currentNode -> list -> head -> ingredient, currentIngredient -> ingredientName);
            Batch* currentBatch = currentNode -> list -> head;
            if(cmp == 0) {
                last = NULL;
                //se per preparare 1 torta mi servono 2 uova. per prepararne X mi servono 2*X uova
                int quantityToFind = currentIngredient -> quantity * quantityOrder;
                while(!ingredientFound && currentBatch != NULL) {

                    //entro nell'if solo se non ho sistemato il batch con il metodo fix
                    if(!fixBatchWareHouse(currentBatch, last, currentNode)){
                        //se avevo modificato quantityLeft ma poi non ho realmente tolto gli ingredienti dal magazzino rispristino il valore iniziale
                        currentBatch -> quantityLeft = currentBatch -> quantity;
                        if(currentBatch -> expiration > currentTime) {
                            currentBatch -> quantityLeft = currentBatch -> quantity - quantityToFind;
                            quantityToFind = quantityToFind - currentBatch -> quantity;
                            if(quantityToFind <= 0)
                                ingredientFound = true;
                        }else {
                            //cancello nodo scaduto
                            currentBatch -> quantityLeft = 0;
                        }
                        last = currentBatch;
                        currentBatch = currentBatch -> next;
                    }
                }

                if(ingredientFound){
                    //sommo i pesi degli ingredienti
                    order -> weight = order -> weight + (currentIngredient -> quantity * quantityOrder);
                    currentIngredient = currentIngredient -> next;
                    ingredientFound = false;
                }else {
                    order -> weight = 0;
                    orderPrepared = false;
                }

            }else{
                last = NULL;
                while(currentBatch != NULL) {
                    if(!fixBatchWareHouse(currentBatch, last, currentNode)) {
                        last = currentBatch;
                        currentBatch = currentBatch -> next;
                    }
                }
                if(cmp > 0) {
                    //non è presente alcun lotto del ingrediente cercato
                    orderPrepared = false;
                }
            }
        }
        currentNode = currentNode -> next;
    }
    //in base a orderPrepared capisco se ho trovato tutti gli ingredienti oppure no
    //prima di restituire true devo cancellare tutti i batch che utilizzo per preparare l'ordine
    wareHouseListofLists -> modified = false;
    if(orderPrepared && currentIngredient == NULL) {
        return true;
    }else {
        order -> weight = 0;
        return false;
    }
}

bool checkIfIngredientIsPresentInWareHouse(ListOfList* wareHouseListofLists, Order* order, int currentTime) {

    if(wareHouseListofLists -> modified == true) {
        return checkIfIngredientIsPresentInModifiedWareHouse(wareHouseListofLists, order, currentTime);
    }else {
        return checkIfIngredientIsPresentInNotModifiedWareHouse(wareHouseListofLists, order, currentTime);
    }
}

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

void prepareOrder(Queue* waitQueue, Queue* readyQueue, ListOfList* listOfLists, int currentTime) {

    //scorro tutta la queue e verifico se ogni ordine è preparabile o no
    //se un ordine è preparabile rimuovo gli ingredienti dai batch e sposto l'ordine nella coda degli ordini pronti
    Order* currentOrder = waitQueue -> head;
    Order* last = NULL;
    bool modifiedNow = false;

    while (currentOrder != NULL) {

        //itero sugli ingredienti e controllo con checkIfIngredientIsPresentInWarehouse se sono tutti presenti
        //se sono tutti presenti sposto l'ordine in readyQueue e rimuovo gli ingredienti dal magazzino
        if(checkIfIngredientIsPresentInWareHouse(listOfLists, currentOrder, currentTime)) {
            modifiedNow = true;
            //sistemo lista waitQueue
            if(last == NULL){
                waitQueue -> head = currentOrder -> next;
            }else {
                last -> next = currentOrder -> next;
            }
            //sposto currentOrder da wait a ready
            appendOrderInReadyQueue(currentOrder, readyQueue);
        }else {
            //se invece non sono tutti presenti non faccio nulla a currentOrder (per farlo conviene creare una nuova coda temporanea se no continuo a iterare all'infinito)
            //appendOrderInWaitQueue(currentOrder, waitQueue);
        }

        last = currentOrder;
        currentOrder = currentOrder -> next;
    }
    //se almeno un ordine viene processato pongo il magazzino a modificato altrimenti a false
    if(modifiedNow == true) {
        listOfLists -> modified = true;
    }else {
        listOfLists -> modified = false;
    }
}

bool prepareSingleOrder(ListOfList* listofLists, Order* order, int currentTime) {

    bool modifiedNow = false;
    if(checkIfIngredientIsPresentInWareHouse(listofLists, order, currentTime)) {
        modifiedNow = true;
    }
    //se almeno un ordine viene processato pongo il magazzino a modificato altrimenti a false
    if(modifiedNow == true) {
        listofLists -> modified = true;
    }else {
        listofLists -> modified = false;
    }
    return modifiedNow;
}

void removeNewline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

void removeRecipeFromList(char* recipeName, RecipeList* recipeList, Queue* readyQueue) {

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
            if(!checkIfRecipeIsPresentInReadyQueue(readyQueue, recipeName)) {
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

void fillVan(int capacity, Queue* readyQueue) {

    //non ci sono ordini pronti
    if(readyQueue -> head == NULL) {
        printf("camioncino vuoto\n");
        return;
    }
    int vanCapacity = capacity;
    Order* currentOrder = readyQueue -> head;
    Order* temp = NULL;
    while (currentOrder != NULL && vanCapacity > 0) {

        if(vanCapacity > currentOrder -> weight) {
            //l'ordine viene caricato sul van
            vanCapacity = vanCapacity - currentOrder -> weight;
            printf("%d %s %d\n", currentOrder -> time, currentOrder -> recipeName, currentOrder -> quantity);
            temp = currentOrder;
            currentOrder = currentOrder -> next;
            free(temp);
        }else {
            //l'ordine non ci sta sul van, passo a quello successivo
            currentOrder = currentOrder -> next;
        }
    }
    readyQueue -> head = currentOrder;
}

//
//FUNZIONI UTILS
//

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
    ListOfList* listOfLists = createListOfLists();
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
            //controllo se gli ordini in attesa possono essere preparati
            prepareOrder(waitQueue, readyQueue, listOfLists, currentTime);
            //lo riempo in base alla sua capacity prendendo gli ordini da readyQueue
            fillVan(capacity, readyQueue);
        }
        commandArgumentHolder = strtok(commandBuffer, COMMAND_ARGUMENTS_DELIMITER);
        tmp = UTILS_hashString(commandArgumentHolder);
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
                    appendIngredientToList(nodeIngredient, recipe -> ingredientList);
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
            removeRecipeFromList(recipeName, recipeList, readyQueue);
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
                insertBatchInWareHouse(listOfLists, batch);
            }
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
                if(prepareSingleOrder(listOfLists, order, currentTime)) {
                    //posso preparare subito l'ordine quindi lo metto in readyQueue
                    //printf("-----------preparo ordine %s-----------\n", order ->recipeName);
                    appendOrderInReadyQueue(order, readyQueue);
                }else {
                    //printf("-----------metto in attesa %s----------\n", order -> recipeName);
                    //altrimenti lo metto in waitQueue
                    appendOrderInWaitQueue(order, waitQueue);
                }
            }else {
                printf("rifiutato\n");
            }
            break;
        default:
            return;
            break;
        }
        currentTime++;
    }
    //ultima volta che arriva il van, dopo l'ultimo comando
    if(currentTime % periodicity == 0) {
        //arriva il furgone. lo riempo in base alla sua capacity prendendo gli ordini da readyQueue
        fillVan(capacity, readyQueue);
    }
}


int main(void) {
    clock_t start, end;

    start = clock();
    printf("Hello, World!\n");

    UTILS_commandsHandler();

    end = clock();

    printf("%f\n", ((double) (end - start)) / CLOCKS_PER_SEC);

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