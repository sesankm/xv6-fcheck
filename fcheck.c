#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>
#include "include/types.h"
#include "include/fs.h"

#define BLOCK_SIZE (BSIZE)

// borrowed this function from mkfs.c
void rsect(int fsfd, uint sec, void *buf)
{
  if(lseek(fsfd, sec * 512L, 0) != sec * 512L){
    perror("lseek");
    exit(1);
  }
  if(read(fsfd, buf, 512) != 512){
    perror("read");
    exit(1);
  }
}


int
main(int argc, char *argv[])
{
  int i,n,fsfd;
  char *addr;
  struct dinode *dip;
  struct superblock *sb;
  struct dirent *de; // used to store dir entries
  uint ind[NINDIRECT]; // used to store indirect blocks
  // used for iterating
  int k;
  int l;
  int j;

  if(argc < 2){
    fprintf(stderr, "Usage: fcheck fs.img ...\n");
    exit(1);
  }


  fsfd = open(argv[1], O_RDONLY);
  if(fsfd < 0){
	printf("image not found\n");
    exit(1);
  }

  struct stat foo;
  fstat(fsfd, &foo); // use fstat to get filesize
  addr = mmap(NULL, foo.st_size, PROT_READ, MAP_PRIVATE, fsfd, 0);
  if (addr == MAP_FAILED){
	perror("mmap failed");
	exit(1);
  }
  /* read the super block */
  sb = (struct superblock *) (addr + 1 * BLOCK_SIZE);

  /* read the inodes */
  dip = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE); 

  int* refd = malloc(sizeof(int) * sb->ninodes); // mapping for all inodes that where referenced in a directory
  for(i = 0; i < sb->ninodes; i++){
	  // initalize referenced array to all 0's
	  refd[i] = 0;
  }

  // populate refd array - if inode was referened in any directory, refd[i] will = 1
  for(i = ROOTINO; i < sb->ninodes; i++){
  	  if(dip[i].type != 1)
      	continue;
      de = (struct dirent *) (addr + (dip[i].addrs[0])*BLOCK_SIZE);
      n = dip[i].size/sizeof(struct dirent);
      for(j = 0; j < n; j++){
        refd[de[j].inum] = 1;
      }
  }

  // check 3 - checking if root directory exists
  de = (struct dirent *) (addr + (dip[ROOTINO].addrs[0])*BLOCK_SIZE);
  // check the names of the first and second directory entires
  // root directory should have . and .. as first and second and should have inode number of 1
  if(strcmp(de[0].name, ".") != 0 || strcmp(de[1].name, "..") != 0 || de[0].inum != 1 || de[1].inum != 1){ 
	  // if any of this is false throw an error
      fprintf(stderr, "ERROR: root directory does not exist.\n");
      exit(1);
  }

  for(i = ROOTINO; i < sb->ninodes; i++){
	  int type = (int) dip[i].type;
	  // check 1 - check if each inode has a valid type
	  if(type != 0 && type != 1 && type != 2 && type != 3){ 
		  fprintf(stderr, "ERROR: bad inode.\n");
		  exit(1);
	  }
	  // check 2 - check if all addresses within range
	  if(dip[i].type > 0 && dip[i].type <= 3){
		  for(j = 0; j < NDIRECT + 1; j++){ 
			  if(dip[i].addrs[j] > sb->nblocks || dip[i].addrs[j] < 0){ // check if the direct address exceeds fs size
				  fprintf(stderr, "ERROR: bad direct address in inode.\n");
				  exit(1);
			  }
		  }
		  // use rsect function to get the indirect addresses for the current inode
		  rsect(fsfd, dip[i].addrs[NDIRECT], (char*)ind);
		  for(j = 0; j < NINDIRECT; j++){
			  if(ind[j] > sb->nblocks || ind[j] < 0){ // check if indirect address exceeds fs size
				  fprintf(stderr, "ERROR: bad indirect address in inode.\n");
				  exit(1);
			  }
		  }
	  }

	  // check 4 - make sure all directories have parent and current references
	  if(type == 1){ 
	  	  // iterate dirents
		  de = (struct dirent *) (addr + (dip[i].addrs[0])*BLOCK_SIZE);
		  // check the names of dirents for index 0 and 1
		  // index 0 should point to current (.) and index 1 should point to parent
		  if(strcmp(de[0].name, ".") != 0 || strcmp(de[1].name, "..") != 0 || de[0].inum != i){
			  fprintf(stderr, "ERROR: directory not properly formatted.\n");
			  exit(1);
		  }
	  }

	  // check 5 check if used addresses are marked as used in bitmap
	  if(type != 0){
	      // get the indirect blocks for current inode
		  rsect(fsfd, dip[i].addrs[NDIRECT], (char*)ind);
		  for(j = 0; j < NDIRECT; j++){ // check if all direct addrs are marked in bitmap
			  char* block  = addr + (BBLOCK(dip[i].addrs[j], sb->ninodes)) * BSIZE;
			  if(!(block[dip[i].addrs[j] / 8] & (0x1 << (dip[i].addrs[j] % 8)))){
				  fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
				  exit(1);
			  }
		  }
		  for(j = 0; j < NINDIRECT; j++){ // same thing for indirect addrs
			  char* block  = addr + (BBLOCK(ind[j], sb->ninodes)) * BSIZE;
			  if(!(block[ind[j] / 8] & (0x1 << (ind[j] % 8)))){
				  fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
				  exit(1);
			  }
		  }
	  }

	  // checks 6-8 are outside of this loop
	  
	  // check 9 if inode is in use, a directory should reference it
	  if(dip[i].type > 0 && i != ROOTINO){
		  int found = 0; // bool for found inode
		  for(j = 0; j < sb->ninodes; j++){ // iterate all inodes
			  if(i == j || dip[j].type != 1) // if i = j its the same inode, and the type should be a directory
				  continue;
			  de = (struct dirent *) (addr + (dip[j].addrs[0])*BLOCK_SIZE);
			  n = dip[j].size/sizeof(struct dirent);
			  for (k = 0; k < n; k++,de++){ // traverse directory entries
				  if(de->inum == i && i != 0){ // if this directory refernces the current inode, break the loop
					  found = 1;
					  break;
				  }
			  }
			  if(found){
				  break;
			  }
		  }
		  // if we've traversed every dirent and still haven't found reference to current node show an error
		  if(!found){
			  fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
			  exit(1);
		  }
	  }

	  // check 10  - populated refd array earlier, check if the current inode is refd using this array
	  if(dip[i].type == 0 && refd[i] != 0){
		  fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
	  }

	  // check 11 - check that the reference counts match the number of links for a file
	  int count = 0; // counter
	  if(type == 2){
		  for(j = 0; j < sb->ninodes; j++){
			  if(i == j || dip[j].type != 1)
				  continue;
			  de = (struct dirent *) (addr + (dip[j].addrs[0])*BLOCK_SIZE);
			  n = dip[j].size/sizeof(struct dirent);
			  for (k = 0; k < n; k++,de++){ //traverse dirents
				  if(de->inum == i) // if dirent's inum is same as current inum inc count
					  count++;
				  if(count > dip[i].nlink){ // if count goes over the number of links, show an error
					  fprintf(stderr,"ERROR: bad reference count for file.\n");
					  exit(1);
				  }

			  }
		  }
		  if(count < dip[i].nlink){ // if count ends up being less than the number of links, also show an error
			  fprintf(stderr,"ERROR: bad reference count for file.\n");
			  exit(1);
		  }
	  }

	  // check 12 make sure all directories are only refernced in one other directory
	  if(type == 1){
	      int count = 0; // keep track of number of refernces for current inode
	      for(j = 0; j < sb->ninodes; j++){ // iterate all inodes
	    	  if(i == j || dip[j].type > 1) // if the type isnt a dir, then move to next iteration
	    		  continue;
	    	  de = (struct dirent *) (addr + (dip[j].addrs[0])*BLOCK_SIZE);
	    	  n = dip[j].size/sizeof(struct dirent);
	    	  for(k = 0; k < n; k++,de++){ // iterate dirents
				  if(de->inum == i && i != ROOTINO && strcmp(de->name,"..") != 0){ // if the inode number matches current inode inc count
					  count++;
				  }
	    		  if(count > 1){ // if there are more than 1 references to curr inode (directory) then show error
	    		  	fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
	    			exit(1);
	    		  }
	    	  }
	      }
	  }
  }

  // check 6
  for(j = (BBLOCK(sb->size, sb->ninodes)) + 1; j < (BBLOCK(sb->size, sb->ninodes) + 1) + sb->nblocks; j++){
	  char* block  = addr + (BBLOCK(j, sb->ninodes)) * BSIZE;
	  // if the block isn't in use move on
	  if(!(block[j / 8] & (0x1 << (j % 8)))){
		  continue;
	  }
	  int found = 0;
	  // for each inode look for address that matches current block
	  for(i = 0; i < sb->ninodes; i++){
		  if(dip[i].type == 0)
			  continue;
		  for(k = 0; k < NDIRECT + 1; k++){
			  if(dip[i].addrs[k] == j){
				  found = 1;
				  break;
			  }
		  }

		  rsect(fsfd, dip[i].addrs[NDIRECT], (char*)ind);
		  for(k = 0; k < NINDIRECT; k++){
			  if(ind[k] == j){
				  found = 1;
				  break;
			  }
		  }
	  }
	  // if a match was found throw an error
	  if(!found){
		  fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
		  exit(1);
	  }
  }

  // checks 7 and 8 - direct and indirect address only used once
  // iterates through every direct and indirect address and makes sure there isn't a copy
  for(i = 0; i < sb->ninodes; i++){
	  for(j = 0; j < sb->ninodes; j++){
		  if(dip[i].type < 1 || dip[j].type < 1 || i == j)
			  continue;
		  // compare direct blocks for inode i and j
		  for(k = 0; k < NDIRECT; k++){
			  for(l = 0; l < NDIRECT; l++){
				  if(dip[i].addrs[k] == dip[j].addrs[l] && dip[j].addrs[l] != 0){
					  fprintf(stderr, "ERROR: direct address used more than once.\n");
					  exit(1);
				  }
			  }
		  }
		  uint ind1[NINDIRECT]; // indirects for inode i
		  uint ind2[NINDIRECT]; // indirects for inode j
		  rsect(fsfd, dip[i].addrs[NDIRECT], (char*)ind1);
		  rsect(fsfd, dip[j].addrs[NDIRECT], (char*)ind2);
		  for(k = 0; k < NINDIRECT; k++){
			  for(l = 0; l < NINDIRECT; l++){
				  if(dip[j].type < 1 || dip[j].type < 1 || k == l)
					  continue;
				  // if there are matching elements from ind1 and ind2, show an error
				  if(ind1[k] == ind2[l] && ind1[k] != 0){
					  fprintf(stderr, "ERROR: indirect address used more than once.\n");
					  exit(1);
				  }
			  }
		  }
	  }
  }

  exit(0);
}

