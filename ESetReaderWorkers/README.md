This project is for measuring read workload of ESetStore.
Edit ThreadWorker.c and change kValue if you want to change the value of k
We set the file size as 4 MB. If you want to change the file size, you need to edit ThreadWorker.c.

Complie:run make

Run:./threadWorker 1 256 2

File from file1 to file256 and usge 2 threads
