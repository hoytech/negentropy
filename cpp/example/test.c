#include <stdint.h>
#include <stdio.h>

#include "../negentropy_wrapper.h"

int main(){
   void* st = storage_new("","");
   if(st == NULL){
    perror("failed to create storage");
   }
   void* ngn_inst = negentropy_new(st, 153600);
   if(ngn_inst == NULL){
    perror("failed to create negentropy instance");
   }

   const char* output = negentropy_initiate(ngn_inst);
   if(ngn_inst == NULL){
    perror("failed to initiate negentropy instance");
   }
   printf("initiated negentropy successfully with output %s \n", output);
   
}