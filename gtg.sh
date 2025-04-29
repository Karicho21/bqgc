#!/bin/bash
#SBATCH --job-name=gtg
#SBATCH --partition=Centaurus
#SBATCH --time=01:00:00
#SBATCH --mem=32G

record="record_8.txt"

record16="record_16.txt"

./gtg "Tom Hanks" 1 8 >> $record
./gtg "Tom Hanks" 2 8 >> $record
./gtg "Tom Hanks" 3 8 >> $record
./gtg "Tom Hanks" 4 8 >> $record

./gtg "Tom Hanks" 1 16 >> $record16
./gtg "Tom Hanks" 2 16 >> $record16
./gtg "Tom Hanks" 3 16 >> $record16
./gtg "Tom Hanks" 4 16 >> $record16
