#!/bin/bash

sudo modprobe fbtft_device -r
sudo modprobe fb_ssd1351 -r
sudo modprobe fbtft -r
