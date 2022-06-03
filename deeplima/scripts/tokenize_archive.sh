#!/bin/bash

INPUT_FILE=$1
OUTPUT_DIR=$2
TOK_MODEL=$3
LNG=$4

xzcat $INPUT_FILE | deeplima-eigen --tok-model=$TOK_MODEL --thread 4 --output-format=horizontal > $OUTPUT_DIR/${LNG}.tok.txt