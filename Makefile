#!/bin/sh

fatread:readfat.c
	$(CC) -g -Wall -o $@ $^ 

clean:
	rm fatread
