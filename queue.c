#include "queue.h"

// Initialize the queue
void init_queue(MessageQueue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

// Enqueue a message
void enqueue_message(MessageQueue* queue, MessageType type, int x, int y, int extra_x, int extra_y, void* pointer) {
    Message* new_msg = malloc(sizeof(Message));
    new_msg->type = type;
    new_msg->data_x = x;
    new_msg->data_y = y;
    new_msg->extra_x = extra_x;
    new_msg->extra_y = extra_y;
    new_msg->pointer = pointer;
    new_msg->next = NULL;

    pthread_mutex_lock(&queue->lock);
    if (queue->tail == NULL) {
        queue->head = queue->tail = new_msg;
    } else {
        queue->tail->next = new_msg;
        queue->tail = new_msg;
    }
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
}

// Priority enqueue a message
void priority_enqueue_message(MessageQueue* queue, MessageType type, int x, int y, int extra_x, int extra_y, void* pointer) {
    Message* new_msg = malloc(sizeof(Message));
    new_msg->type = type;
    new_msg->data_x = x;
    new_msg->data_y = y;
    new_msg->extra_x = extra_x;
    new_msg->extra_y = extra_y;
    new_msg->pointer = pointer;
    new_msg->next = NULL;

    pthread_mutex_lock(&queue->lock);

    // Insert the message at the head of the queue
    if (queue->head == NULL) {
        queue->head = queue->tail = new_msg;
    } else {
        new_msg->next = queue->head;
        queue->head = new_msg;
    }

    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
}

// Dequeue a message
Message* dequeue_message(MessageQueue* queue) {
    pthread_mutex_lock(&queue->lock);
    while (queue->head == NULL) {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }

    Message* msg = queue->head;
    queue->head = queue->head->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    pthread_mutex_unlock(&queue->lock);
    return msg;
}

// Cleanup the queue
void cleanup_queue(MessageQueue* queue) {
    pthread_mutex_lock(&queue->lock); // Lock the queue to ensure thread safety

    Message* current = queue->head;
    while (current != NULL) {
        Message* next = current->next; // Save the next message
        free(current);                 // Free the current message
        current = next;                // Move to the next message
    }

    // Reset the queue
    queue->head = NULL;
    queue->tail = NULL;

    pthread_mutex_unlock(&queue->lock); // Unlock the queue
}