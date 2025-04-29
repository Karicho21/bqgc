This is for ITSC 4145 Extra credit Blocking Queue Graoh Crawler

But before you begin, make sure you have access to

- A Linux-based system with SLURM workload management
- gcc compiler (this is critical to execute the program)
- Access to hpc lab computer Centaurus
- UNC Charlotte VPN if you are outside of campus or not using "eduroam" network


Steps to compile and experiment:
1. Connecting hpc lab computer by "ssh hpc-student.charlotte.edu -l your-username"
2. Authenticate with Duo
3. Type "g++ bg.cpp -o gtg -I ~/rapidjson/include -lcurl" to create executable file named "gtg".
4. Schedule the job by "sbatch gtg.sh"
5. Outcome should be something like "Submitted batch job [????]". It also creates  file called "record_8.txt" and "record_16.txt" it will have all the runtime log.
6. Wait a bit for command to finish running and record the time it takes.
7.  If you would like a csv file recording the time, type "sbatch npf.sh > timelog.csv". It will schedule the job and record the time onto csv file called timelog.csv. You can name the file whatever you desire, but this timelog is what I named.

Sample output:
- ./gtg "Tom Hanks" 1 8 -> Time passed: 0.179787 seconds, Nodes found: 40
- ./gtg "Tom Hanks" 2 8 -> Time passed: 0.719031 seconds, Nodes found: 888
- ./gtg "Tom Hanks" 3 8 -> Time passed: 10.1929 seconds, Nodes found: 5911
- ./gtg "Tom Hanks" 4 8 -> Time passed: 65.9927 seconds, Nodes found: 29790
 
- ./gtg "Tom Hanks" 1 16 -> Time passed: 0.174537 seconds, Nodes found: 40 (x1.03 speed up)
- ./gtg "Tom Hanks" 2 16 -> Time passed: 0.547544 seconds, Nodes found: 888 (x1.31 speed up)
- ./gtg "Tom Hanks" 3 16 -> Time passed: 5.18631 seconds, Nodes found: 5911 (x1.97 speed up)
- ./gtg "Tom Hanks" 4 16 -> Time passed: 33.2193 seconds, Nodes found: 29790 (x1.99 speed up)

Time varies each run. You won't get same time each run but should be similar. (because sometimes centaurus is busy)

Conclusion: 

When depth is low number like 1 or 2, it is harder to observe the speed up using more threads, simply because there are not many nodes to go through. However, when depth gets larger and there is an increase of nodes to go through, speed up makes significant difference in runtime. For this program, speed up correlates to the increase in number of threads. If number of threads triples, the speed up is =< x3 therefore runtime is 1/3 of original time.

For reference: 
- ./gtg "Tom Hanks" 4 32 -> Time passed: 16.9189 seconds
- ./gtg "Tom Hnaks" 4 64 -> Time passed: 8.63938 seconds
