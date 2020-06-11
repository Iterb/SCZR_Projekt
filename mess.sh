#!/bin/bash
# since Bash v4
for i in {1..300}
do
	sleep 1
     sudo ./sobel images/pic.png results/
done
