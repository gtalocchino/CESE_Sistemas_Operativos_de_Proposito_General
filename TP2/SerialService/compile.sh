#!/usr/bin/env bash

gcc serial_service.c SerialManager.c -pthread -o serial_service

