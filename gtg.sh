#!/bin/bash
#SBATCH --job-name=gtg
#SBATCH --partition=Centaurus
#SBATCH --time=01:00:00
#SBATCH --mem=32G

record="record.txt"

./gtg "Tom Hanks" 1 >> $record
./gtg "Tom Hanks" 2 >> $record
./gtg "Tom Hanks" 3 >> $record
./gtg "Tom Hanks" 4 >> $record
