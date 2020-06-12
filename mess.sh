#!/bin/bash
# since Bash v4
for i in {1..100}
do
	sleep 10
     sudo ./sobel images/pic.png results/
done
