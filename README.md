# NSDI19Spring
RVH

### compile
>```
>$ make
>```

### run
>```
>$ ./main -r <ruleset> -p <trace> -x <split points> -y <split points>
>```
>>-r : ruleset files in the classbench format<br>
>>-p : trace files in the classbench format<br>
>>-x, -y : split points eparated by commas for the x/y field (e.g., -x 0,4,16,24 splits the x field as [0,4) [4,16) [16,24) [24,33))

### clean
>```
>$ make clean
>```
