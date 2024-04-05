/*
* Copyright ©2024 Hannah C. Tang.  All rights reserved.  Permission is
* hereby granted to students registered for University of Washington
* CSE 333 for use solely during Spring Quarter 2024 for purposes of
* the course.  No other use, copying, distribution, or modification
* is permitted without prior written consent. Copyrights for
* third-party components of this work must be honored.  Instructors
* interested in reusing these course materials should contact the
* author.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "CSE333.h"
#include "HashTable.h"
#include "LinkedList.h"
#include "HashTable_priv.h"

///////////////////////////////////////////////////////////////////////////////
// Internal helper functions.
//
#define INVALID_IDX -1

// Grows the hashtable (ie, increase the number of buckets) if its load
// factor has become too high.
static void MaybeResize(HashTable *ht);

int HashKeyToBucketNum(HashTable *ht, HTKey_t key) {
    return key % ht->num_buckets;
}

// Deallocation functions that do nothing.  Useful if we want to deallocate
// the structure (eg, the linked list) without deallocating its elements or
// if we know that the structure is empty.
static void LLNoOpFree(LLPayload_t freeme) { }
static void HTNoOpFree(HTValue_t freeme) { }

// Finds the given key-value pair within a list and will remove it if necessary
//
// Arguments:
// - list: the list to search through
// - searchKey: the key to find
// - isRemove: if true function will remove the node from the list
//
// Returns:
// - pointer to the HTKeyValue_t in the node if found
// - NULL if not found
HTKeyValue_t* getKeyValue(LinkedList* list, HTKey_t searchKey, bool isRemove);

// Moves iter to the next bucket with an element
//
// Arguments:
// - iter: the iterator to move. If there are no more elements then
//         the iter's bucket_idx will be set to -1 and its
//         bucket_itr will be set to NULL.
//
// Returns:
// - true: if the iter was successfully moved to a new element
// - false: if there were no more elements and the iter
//          moved past the table
bool getNextBucket(HTIterator *iter);


///////////////////////////////////////////////////////////////////////////////
// HashTable implementation.

HTKey_t FNVHash64(unsigned char *buffer, int len) {
    // This code is adapted from code by Landon Curt Noll
    // and Bonelli Nicola:
    //     http://code.google.com/p/nicola-bonelli-repo/
    static const uint64_t FNV1_64_INIT = 0xcbf29ce484222325ULL;
    static const uint64_t FNV_64_PRIME = 0x100000001b3ULL;
    unsigned char *bp = (unsigned char *) buffer;
    unsigned char *be = bp + len;
    uint64_t hval = FNV1_64_INIT;

    // FNV-1a hash each octet of the buffer.
    while (bp < be) {
        // XOR the bottom with the current octet.
        hval ^= (uint64_t) * bp++;
        // Multiply by the 64 bit FNV magic prime mod 2^64.
        hval *= FNV_64_PRIME;
    }
    return hval;
}

HashTable* HashTable_Allocate(int num_buckets) {
    HashTable *ht;
    int i;

    Verify333(num_buckets > 0);

    // Allocate the hash table record.
    ht = (HashTable *) malloc(sizeof(HashTable));
    Verify333(ht != NULL);

    // Initialize the record.
    ht->num_buckets = num_buckets;
    ht->num_elements = 0;
    ht->buckets = (LinkedList **) malloc(num_buckets * sizeof(LinkedList *));
    Verify333(ht->buckets != NULL);
    for (i = 0; i < num_buckets; i++) {
        ht->buckets[i] = LinkedList_Allocate();
    }

    return ht;
}

void HashTable_Free(HashTable *table,
                    ValueFreeFnPtr value_free_function) {
    int i;

    Verify333(table != NULL);

    // Free each bucket's chain.
    for (i = 0; i < table->num_buckets; i++) {
        LinkedList *bucket = table->buckets[i];
        HTKeyValue_t *kv;

        // Pop elements off the chain list one at a time.  We can't do a single
        // call to LinkedList_Free since we need to use the passed-in
        // value_free_function -- which takes a HTValue_t, not an LLPayload_t --
        // to free the caller's memory.
        while (LinkedList_NumElements(bucket) > 0) {
            Verify333(LinkedList_Pop(bucket, (LLPayload_t *)&kv));
            value_free_function(kv->value);
            free(kv);
        }
        // The chain is empty, so we can pass in the
        // null free function to LinkedList_Free.
        LinkedList_Free(bucket, LLNoOpFree);
    }

    // Free the bucket array within the table,
    // then free the table record itself.
    free(table->buckets);
    free(table);
}

int HashTable_NumElements(HashTable *table) {
    Verify333(table != NULL);
    return table->num_elements;
}

bool HashTable_Insert(HashTable *table,
                    HTKeyValue_t newkeyvalue,
                    HTKeyValue_t *oldkeyvalue) {
    int bucket;
    LinkedList *chain;

    Verify333(table != NULL);
    MaybeResize(table);

    // Calculate which bucket and chain we're inserting into.
    bucket = HashKeyToBucketNum(table, newkeyvalue.key);
    chain = table->buckets[bucket];

    // STEP 1: finish the implementation of InsertHashTable.
    // This is a fairly complex task, so you might decide you want
    // to define/implement a helper function that helps you find
    // and optionally remove a key within a chain, rather than putting
    // all that logic inside here.  You might also find that your helper
    // can be reused in steps 2 and 3.


    bool res = false;
    // Find key value pair and remove it
    HTKeyValue_t* oldkv = getKeyValue(chain, newkeyvalue.key, true);

    if (oldkv != NULL) {
        // Key was found in the chain
        // The node was removed so don't increase size
        *oldkeyvalue = *oldkv;
        free(oldkv);
        res = true;
    } else {
        // Key was not found in the chain
        table->num_elements+=1;
    }

    // Add a new node with the newkeyvalue as the payload
    HTKeyValue_t* newKVptr = (HTKeyValue_t*)malloc(sizeof(newkeyvalue));
    *newKVptr = newkeyvalue;
    LinkedList_Push(chain, (LLPayload_t) newKVptr);
    return res;
}


bool HashTable_Find(HashTable *table,
                    HTKey_t key,
                    HTKeyValue_t *keyvalue) {
    Verify333(table != NULL);
    // STEP 2: implement HashTable_Find.

    int bucket;
    LinkedList *chain;

    // Calculate which bucket and chain we're finding into.
    bucket = HashKeyToBucketNum(table, key);
    chain = table->buckets[bucket];

    // Get Key Value and do not delete node if found
    HTKeyValue_t* searchkv = getKeyValue(chain, key, false);
    if (searchkv == NULL) {
        // Key was not found in chain
        return false;
    }
    // Key was found in chain
    *keyvalue = *searchkv;
    return true;
}

bool HashTable_Remove(HashTable *table,
                    HTKey_t key,
                    HTKeyValue_t *keyvalue) {
    Verify333(table != NULL);

    // STEP 3: implement HashTable_Remove.

    int bucket;
    LinkedList *chain;

    // Calculate which bucket and chain we're removing from into.
    bucket = HashKeyToBucketNum(table, key);
    chain = table->buckets[bucket];

    // Find key value and remove if found
    HTKeyValue_t* searchkv = getKeyValue(chain, key, true);
    if (searchkv == NULL) {
        // Key not found in chain
        return false;
    }

    // Key found in chain
    table->num_elements-=1;
    *keyvalue = *searchkv;

    // Free the space allocated for the key-value pair
    free(searchkv);
    return true;
}


///////////////////////////////////////////////////////////////////////////////
// HTIterator implementation.

HTIterator* HTIterator_Allocate(HashTable *table) {
    HTIterator *iter;
    int         i;

    Verify333(table != NULL);

    iter = (HTIterator *) malloc(sizeof(HTIterator));
    Verify333(iter != NULL);

    // If the hash table is empty, the iterator is immediately invalid,
    // since it can't point to anything.
    if (table->num_elements == 0) {
        iter->ht = table;
        iter->bucket_it = NULL;
        iter->bucket_idx = INVALID_IDX;
        return iter;
    }

    // Initialize the iterator.  There is at least one element in the
    // table, so find the first element and point the iterator at it.
    iter->ht = table;
    for (i = 0; i < table->num_buckets; i++) {
        if (LinkedList_NumElements(table->buckets[i]) > 0) {
            iter->bucket_idx = i;
            break;
        }
    }
    Verify333(i < table->num_buckets);  // make sure we found it.
    iter->bucket_it = LLIterator_Allocate(table->buckets[iter->bucket_idx]);
    return iter;
}

void HTIterator_Free(HTIterator *iter) {
    Verify333(iter != NULL);
    if (iter->bucket_it != NULL) {
        LLIterator_Free(iter->bucket_it);
        iter->bucket_it = NULL;
    }
    free(iter);
}


bool HTIterator_IsValid(HTIterator *iter) {
    Verify333(iter != NULL);

    // STEP 4: implement HTIterator_IsValid.
    return iter->bucket_it != NULL && iter->bucket_idx != INVALID_IDX;
}

bool HTIterator_Next(HTIterator *iter) {
    Verify333(iter != NULL);

    // STEP 5: implement HTIterator_Next.
    if (!HTIterator_IsValid(iter)) {
        return false;
    }

    // Should always be valid because if iter is not valid
    // after the LLIterator_Next call we move the iter to
    // the next bucket immediately
    Verify333(LLIterator_IsValid(iter->bucket_it));
    LLIterator_Next(iter->bucket_it);

    bool res = true;
    // If the iter is invalid we can call getNextBucket on it
    if (!LLIterator_IsValid(iter->bucket_it))
        res = getNextBucket(iter);
    return res;
}

bool HTIterator_Get(HTIterator *iter, HTKeyValue_t *keyvalue) {
    Verify333(iter != NULL);

    // STEP 6: implement HTIterator_Get.
    if (!HTIterator_IsValid(iter))
        return false;

    HTKeyValue_t* kv;
    LLIterator_Get(iter->bucket_it, (LLPayload_t*) &kv);
    *keyvalue = *kv;
    return true;  // you may need to change this return value
}

bool HTIterator_Remove(HTIterator *iter, HTKeyValue_t *keyvalue) {
    HTKeyValue_t kv;

    Verify333(iter != NULL);

    // Try to get what the iterator is pointing to.
    if (!HTIterator_Get(iter, &kv)) {
        return false;
    }

    // Advance the iterator.  Thanks to the above call to
    // HTIterator_Get, we know that this iterator is valid (though it
    // may not be valid after this call to HTIterator_Next).
    HTIterator_Next(iter);

    // Lastly, remove the element.  Again, we know this call will succeed
    // due to the successful HTIterator_Get above.
    Verify333(HashTable_Remove(iter->ht, kv.key, keyvalue));
    Verify333(kv.key == keyvalue->key);
    Verify333(kv.value == keyvalue->value);

    return true;
}

static void MaybeResize(HashTable *ht) {
    HashTable *newht;
    HashTable tmp;
    HTIterator *it;

    // Resize if the load factor is > 3.
    if (ht->num_elements < 3 * ht->num_buckets)
        return;

    // This is the resize case.  Allocate a new hashtable,
    // iterate over the old hashtable, do the surgery on
    // the old hashtable record and free up the new hashtable
    // record.
    newht = HashTable_Allocate(ht->num_buckets * 9);

    // Loop through the old ht copying its elements over into the new one.
    for (it = HTIterator_Allocate(ht);
        HTIterator_IsValid(it);
        HTIterator_Next(it)) {
        HTKeyValue_t item, unused;

        Verify333(HTIterator_Get(it, &item));
        HashTable_Insert(newht, item, &unused);
    }

    // Swap the new table onto the old, then free the old table (tricky!).  We
    // use the "no-op free" because we don't actually want to free the elements;
    // they're owned by the new table.
    tmp = *ht;
    *ht = *newht;
    *newht = tmp;

    // Done!  Clean up our iterator and temporary table.
    HTIterator_Free(it);
    HashTable_Free(newht, &HTNoOpFree);
}

HTKeyValue_t* getKeyValue(LinkedList* list, HTKey_t searchKey, bool isRemove) {
    LLIterator* iter = LLIterator_Allocate(list);
    Verify333(iter != NULL);

    // Iterate through list to find node with searchKey
    while (LLIterator_IsValid(iter)) {
        HTKeyValue_t* payload;
        LLIterator_Get(iter, (LLPayload_t*) &payload);

        // If payload's key equals searchKey we have
        // Found the node and can return the payload
        if (payload->key == searchKey) {
            // Remove node if specified
            if (isRemove)
                LLIterator_Remove(iter, LLNoOpFree);
            LLIterator_Free(iter);
            return payload;
        }
        LLIterator_Next(iter);
    }
    // searchKey was not found so return NULL
    LLIterator_Free(iter);
    return NULL;
}

bool getNextBucket(HTIterator *iter) {
    Verify333(!LLIterator_IsValid(iter->bucket_it));
    LinkedList** buckets = iter->ht->buckets;

    // Iterate through buckets until a non-empty one is found
    // or until the index is past the number of buckets
    do {
        iter->bucket_idx++;
        if (iter->bucket_idx >= iter->ht->num_buckets) {
            // Past the # of buckets so return false
            LLIterator_Free(iter->bucket_it);
            iter->bucket_idx = INVALID_IDX;
            iter->bucket_it = NULL;
            return false;
        }
    } while (LinkedList_NumElements(buckets[iter->bucket_idx]) == 0);

    // Non-empty bucket was found.
    // Free the old iterator and allocate a new one
    LLIterator_Free(iter->bucket_it);
    iter->bucket_it = LLIterator_Allocate(buckets[iter->bucket_idx]);
    return true;
}
