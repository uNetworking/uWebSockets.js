#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

/* Free timers is a stack of available timer offsets */
unsigned int freeTimersHead = 0;
unsigned int freeTimers[1000000];

/* Every timer is a bunch of components */
unsigned int componentMultiplierMs[5] = {10, 50, 100, 500, 1000};

struct Timer {
    /* Offsets in timers array */
    unsigned int next, prev;

    /* The components remaining for this untriggered run */
    unsigned int components[5];

    /* We need to track the original Ms in case of repeat timers */
    unsigned int nextOriginalMs;

    /* Overshoot Ms is used to track accumulated overshoot and to remove smallest 10ms when overshot more than 10ms */
    unsigned int overshootMs;

    /* What list the timer is in */
    unsigned int componentOffset;
};

/* Timers are strides over an integer array */
struct Timer timers[1000000];

/* Timer to registered callback */
void (*timerCallbacks[1000000])();

/* Per component timer doubly linked list (head) */
unsigned int timerListHead[5] = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};

void cb() {
    printf("A timer triggered!\n");
}

void init() {
    freeTimersHead = 0;
    for (int i = 0; i < 1000000; i++) {
        freeTimers[i] = i;
    }
    for (int i = 0; i < 5; i++) {
        timerListHead[i] = UINT_MAX;
    }
}

unsigned int allocateTimer() {
    freeTimersHead++;
    //printf("Allocating timer: %d\n", freeTimers[freeTimersHead - 1]);
    return freeTimers[freeTimersHead - 1];
}

void freeTimer(unsigned int timer) {
    //printf("Freeing timer: %d\n", timer);
    freeTimersHead--;
    freeTimers[freeTimersHead] = timer;
}

unsigned int divideComponents(unsigned int components[5], unsigned int *biggestSetComponent, unsigned int ms) {
    /* Start on fifth component */
    int biggestComponent = 4;

    while (ms > 0 && biggestComponent >= 0) {
        /* Can we divide by this component? */
        if (ms >= componentMultiplierMs[biggestComponent]) {
            components[biggestComponent] = ms / componentMultiplierMs[biggestComponent];
            ms -= components[biggestComponent] * componentMultiplierMs[biggestComponent];

            if (*biggestSetComponent < biggestComponent) {
                *biggestSetComponent = biggestComponent;
            }
        }
        biggestComponent--;
    }

    unsigned int overshoot = 0;
    if (ms) {
        components[0]++;
        overshoot = 10 - ms;
    }

    /* Return the overshoot */
    return overshoot;
}

void addTimerToList(unsigned int timer, unsigned int componentOffset) {
    unsigned int head = timerListHead[componentOffset];
    timers[timer].next = head;
    timers[timer].prev = UINT_MAX;
    if (head != UINT_MAX) {
        timers[head].prev = timer;
    }
    timerListHead[componentOffset] = timer;

    /* Track what list the timer is in */
    timers[timer].componentOffset = componentOffset;
}

/* Returns the next timer in the list or UINT_MAX */
unsigned int removeTimerFromList(unsigned int timer, unsigned int componentOffset) {
    unsigned int prev = timers[timer].prev;
    unsigned int next = timers[timer].next;

    if (prev != UINT_MAX) {
        timers[prev].next = next;
    } else {
        timerListHead[componentOffset] = next;
    }

    if (next != UINT_MAX) {
        timers[next].prev = prev;
    }

    return next;
}

unsigned int moveTimerToList(unsigned int timer, unsigned int newComponentOffset) {
    unsigned int currentComponentOffset = timers[timer].componentOffset;
    unsigned int nextTimer = removeTimerFromList(timer, currentComponentOffset);
    addTimerToList(timer, newComponentOffset);
    return nextTimer;
}

/* Trigger a tick of the given component offset, to be called from system timers */
void tick(unsigned int componentOffset) {
    //printf("Tick for %d\n", componentOffset);

    /* Iterate this list, decrementing the timer components */
    unsigned int timerIterator = timerListHead[componentOffset];

    while (timerIterator != UINT_MAX) {
        //printf("Iterating timer %d\n", timerIterator);

        /* This timer needs to move to a higher precision list, or trigger */
        if (!--timers[timerIterator].components[componentOffset]) {
            
            /* Seek to next non-null component or the 0th component */
            unsigned int nextComponentOffsetForTimer = componentOffset;
            while (nextComponentOffsetForTimer && timers[timerIterator].components[nextComponentOffsetForTimer] == 0) {
                nextComponentOffsetForTimer--;
            }

            /* Here we either have a new list to join or we trigger the timer here and now */
            if (timers[timerIterator].components[nextComponentOffsetForTimer]) {
                /* This should return the next timerIterator */
                timerIterator = moveTimerToList(timerIterator, nextComponentOffsetForTimer);
            } else {
                /* This callback must not modify the list (but we can handle that later!) */
                timerCallbacks[timerIterator]();

                /* If the timer itself is removed in the above callback, below removal will fail */

                /* And remove the timer (this should return the next timerIterator) */
                timerIterator = removeTimerFromList(timerIterator, componentOffset);
            }
        } else {
            timerIterator = timers[timerIterator].next;
        }
    }
}

unsigned int setTimeout_(void (*cb)(), unsigned int ms) {
    /* Allocate free timer */
    unsigned int timer = allocateTimer();
    
    /* Divide given ms in components */
    unsigned int biggestSetComponent = 0;
    timers[timer].overshootMs = divideComponents(timers[timer].components, &biggestSetComponent, ms);

    //printf("Overshoot by %u ms\n", timers[timer].overshootMs);
    //printf("Biggest set component is %u\n", biggestSetComponent);

    /* Add the timer to the list of the highest component */
    addTimerToList(timer, biggestSetComponent);

    /* Set the callback */
    timerCallbacks[timer] = cb;

    return timer;
}

void clearTimeout_(unsigned int timer) {
    /* Unlink the timer from its list */
    removeTimerFromList(timer, timers[timer].componentOffset);

    /* Put the timer into free timer circle buffer */
    freeTimer(timer);
}