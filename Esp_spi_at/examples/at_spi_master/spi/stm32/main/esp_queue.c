/*
 * Espressif Systems Wireless LAN device driver
 *
 * Copyright (C) 2015-2022 Espressif Systems (Shanghai) PTE LTD
 * SPDX-License-Identifier: GPL-2.0-only OR Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "esp_queue.h"
#include "FreeRTOS.h"
#include "task.h"

/* create new node */
static q_node_t * new_q_node(void *data)
{
	q_node_t* new_node = (q_node_t*)malloc(sizeof(q_node_t));
	if (!new_node)
		return NULL;
	new_node->data = data;
	new_node->next = NULL;
	return new_node;
}

/* Create app queue */
esp_queue_t* create_esp_queue(void)
{
	esp_queue_t* q = (esp_queue_t*)malloc(sizeof(esp_queue_t));
	if (!q)
		return NULL;

	q->front = q->rear = NULL;
	return q;
}

/* Put element in app queue.
 * Thread-safe: pointer updates are protected by scheduler suspension
 * so that concurrent get/put from different tasks cannot corrupt the
 * linked-list pointers.  malloc() runs outside the critical section. */
int esp_queue_put(esp_queue_t* q, void *data)
{
	q_node_t* new_node = NULL;

	if (!q) {
		printf("q undefined\n");
		return ESP_QUEUE_ERR_UNINITALISED;
	}

	new_node = new_q_node(data);
	if (!new_node) {
		printf("malloc failed in qpp_q_put\n");
		return ESP_QUEUE_ERR_MEMORY;
	}

	vTaskSuspendAll();
	if (q->rear == NULL) {
		q->front = q->rear = new_node;
	} else {
		q->rear->next = new_node;
		q->rear = new_node;
	}
	(void)xTaskResumeAll();
	return ESP_QUEUE_SUCCESS;
}

/* Get element in app queue.
 * Thread-safe: pointer updates are protected by scheduler suspension;
 * the node is freed after the critical section. */
void *esp_queue_get(esp_queue_t* q)
{
	void * data = NULL;
	q_node_t* temp = NULL;

	vTaskSuspendAll();
	if (!q || q->front == NULL) {
		(void)xTaskResumeAll();
		return NULL;
	}

	/* move front one node ahead */
	temp = q->front;
	q->front = q->front->next;

	/* If front is NULL, change rear also as NULL */
	if (q->front == NULL)
		q->rear = NULL;
	(void)xTaskResumeAll();

	data = temp->data;
	free(temp);

	return data;
}

/* Detach all nodes inside the critical section, then free outside. */
void esp_queue_reset(esp_queue_t *q)
{
	q_node_t *temp;
	q_node_t *list_head;

	if (!q)
		return;

	vTaskSuspendAll();
	list_head = q->front;
	q->front = q->rear = NULL;
	(void)xTaskResumeAll();

	while (list_head)
	{
		temp = list_head;
		list_head = list_head->next;
		if (temp->data)
		{
			free(temp->data);
		}
		free(temp);
	}
} // void esp_queue_reset(esp_queue_t *q)


/* Check if there's any element in app queue */
bool esp_queue_check(esp_queue_t* q)
{
	if (!q || q->front == NULL)
		return false;

	return true;
}

void esp_queue_destroy(esp_queue_t** q)
{
	q_node_t* temp = NULL;

	if (!q || !*q)
		return;

	while ((*q)->front) {

		temp = (*q)->front;
		(*q)->front = (*q)->front->next;

		if (temp->data) {
			free(temp->data);
			temp->data = NULL;
		}

		free(temp);
		temp = NULL;
	}

	free(*q);
	*q = NULL;
}
