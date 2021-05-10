#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

int a=0, b=0, c=0, d=0;//predefined variable to print
bool is_vm_full=false;//physical memory full or not
int vm_notfull_index=0;//if vm is not full, put the new page where space is present
int head_fifo=-1;//index of head of queue for FIFO
int head_lru=-1;//index of head of queue for lru case
int clock_hand=0;//initiate clock hand from index 0 for use bit to implement CLOCK
int num_frames; //number of frames to be taken as input
int tot_accesses=0;//total number of memory accesses (for OPT)
int ipt[1048576];//inverted page table of size 2^20 initialised with 0s
//ipt entries' all but last digits are hexadecimal indices of pages in virtual memory
//last digit contains the information about present/dirty/use with the following key
//8=not present, clean, clear; 1=not present, dirty, clear
//2=present, clean, clear    ; 3=present, dirty, clear
//4=not present, clean, use  ; 5=not present, dirty, use
//6=present, clean, use      ; 7=present, dirty, use
//0=memory not once accessed yet so not in address space
char* ins[100000000];//storing all instructions in case of OPT
int nums[100000000];//storing page nums in case of OPT

// functions used
void print_results();
void print_verbose(char* read_page, char* old_page, int replace_type);
void my_print(int vm[]);
int swap_and_update(int vm[], char* read_page, char* strategy, char rw, bool verbose, int access_num);
void insert_entry(int vm[], int ppn, int vpn, char rw);
void swap_entries(int vm[], int vpn1,int vpn2);
void update_entry(int vm[], int ppn, int vpn, char rw, char* strategy);
int access_page (int vm[],char* ppn_hex,char rw, char* strategy, bool verbose, int access_num);


void print_results(){
	printf("Number of memory accesses: %d\n", a);
	printf("Number of misses: %d\n", b);
	printf("Number of writes: %d\n", c);
	printf("Number of drops: %d\n", d);
}

void print_verbose(char* read_page, char* old_page, int replace_type){
	if (replace_type == 0){
		printf("Page %s was read from disk, page %s was written to the disk.\n", read_page, old_page);
	}else{
		printf("Page %s was read from disk, page %s was dropped (it was not dirty).\n", read_page, old_page);
	}
}

void my_print(int vm[]) { //own helper code
	printf("i (dec)\t");
	for (int i=0; i < num_frames && vm[i]; i++) {
		printf("%d\t",i);
	}
	printf("\nvm[i]\t");
	for (int i=0; i < num_frames && vm[i]; i++) {
		printf("%x\t",vm[i]);
	}
	printf("\nipt[]\t");
	for (int i=0; i < num_frames && vm[i]; i++) {
		printf("%x\t",ipt[vm[i]]);
	}
	printf("\n");
}

int swap_and_update(int vm[], char* read_page, char* strategy, char rw, bool verbose, int access_num) {
	int old_ppn=0x00000,new_ppn;
	sscanf(read_page,"%x",&new_ppn);
	int found=0;
	int replace_type=1;
	int vpn=0;
	if (!strcmp(strategy,"OPT")) {
		int last_access=-1;
		bool stop=false;//stop if an index is found not accessed ever again
		for (int i=0; (i < num_frames) && (!stop); i++) {
			for (int j = access_num; j < tot_accesses; j++) {
				if (nums[j]==vm[i]) {
					if (last_access<j) {
						// if ppn is present and is accessed farther than before
						vpn=i;
						last_access=j;
					}
					break;
				}
				if (j==(tot_accesses-1)) { 
					//if a page is not accessed at all, choose it and break loops
					vpn=i;
					stop=true;
					break;
				}
			}
			if (stop) break;
		}
		found=1;
	}
	else if (!strcmp(strategy,"FIFO")) {
		head_fifo=((head_fifo+1)%num_frames);
		vpn=head_fifo;
		found=1;
	}
	else if (!strcmp(strategy,"CLOCK")) {
		vpn=clock_hand;
		while ((ipt[vm[vpn]] >> 2) % 2) {
			ipt[vm[vpn]] ^= (1 << 2);
			vpn=((vpn+1)%num_frames);
		}
		clock_hand=((vpn+1)%num_frames);
		found=1;
	}
	else if (!strcmp(strategy,"LRU")) {
		head_lru=((head_lru+1)%num_frames);
		vpn=head_lru;
		found=1;
	}
	else if (!strcmp(strategy,"RANDOM")) {
		vpn=(rand()%num_frames);
		found=1;
	}
	if (!found) {
		printf("Unrecognised strategy");
		return -1;
	}

	// finding the old page number to be given to verbose
	old_ppn=vm[vpn];
	char* old_ins = malloc(16);
	for (int i = 0; i < tot_accesses-1; i++) {
		if (nums[i]==old_ppn){
			old_ins=ins[i];
			break;
		}
	}
	int length=strlen(old_ins);
	char old_page[length-2];
	strncpy(old_page,old_ins,length-3);
	old_page[length-3]='\0';//found old page number string
	
	// check if writing to disk or dropping page
	if (((ipt[old_ppn]%16==3) || (ipt[old_ppn]%16==7))) {//writing to disk
		replace_type=0;
		c++;//increase write count
	}
	else if (((ipt[old_ppn]%16)==2) || ((ipt[old_ppn]%16)==6))//dropping
		d++;//increase drop count

	if (verbose) print_verbose(read_page,old_page,replace_type);//printing swap
	
	//setting ipt and vm for new page accessed
	if (rw=='W'){
		ipt[new_ppn]=vpn*16+7;
		vm[vpn]=new_ppn;
	}
	else if (rw=='R'){
		ipt[new_ppn]=vpn*16+6;
		vm[vpn]=new_ppn;
	}
	
	//updating old ppn to not present, clear and clean but accessed
	ipt[old_ppn]=(16*(ipt[old_ppn]/16)+8);
	
	return 0;
}

void insert_entry(int vm[], int ppn, int vpn, char rw) {
	if (rw=='W'){
		ipt[ppn]=vpn*16+7;
		vm[vpn]=ppn;
	}
	else if (rw=='R'){
		ipt[ppn]=vpn*16+6;
		vm[vpn]=ppn;
	}
	head_lru=((head_lru+1)%num_frames);
}

void swap_entries(int vm[], int vpn1,int vpn2) {//helper function for LRU
	int ppn1=vm[vpn1];
	int ppn2=vm[vpn2];
	int info1=ipt[ppn1];
	int info2=ipt[ppn2];
	
	vm[vpn1]=ppn2;
	vm[vpn2]=ppn1;
	ipt[ppn1]=(info1%16)+(info2/16)*16;
	ipt[ppn2]=(info2%16)+(info1/16)*16;
}

void update_entry(int vm[], int ppn, int vpn, char rw, char* strategy) {
	if (rw=='W') // already present page made dirty
		ipt[ppn]=vpn*16+7;
	else if (rw=='R'){
		if (ipt[ppn]%2==1) // already present dirty page remain dirty
			ipt[ppn]=vpn*16+7;//
		else ipt[ppn]=vpn*16+6;// already present clean page remains clean
	}
	
	// if strategy=LRU, bring page to head as this is most recent
	if (!strcmp(strategy,"LRU") && vpn!=head_lru) {
		while (vpn!=head_lru) {
			swap_entries(vm,vpn,((vpn+1)%num_frames));
			vpn=((vpn+1)%num_frames);
		}
		head_lru=vpn;
	}
}

int access_page (int vm[],char* ppn_hex,char rw, char* strategy, bool verbose, int access_num) {
	//access_num is the nth memory access which is required for OPT
	int ppn;
	sscanf(ppn_hex,"%x",&ppn);
	if (ipt[ppn]==0) {//cold miss
		b++;//increase miss count;
		if (!is_vm_full) { // insert in a new slot
			vm_notfull_index++;
			if (vm_notfull_index==num_frames) is_vm_full=true;
			int vpn=vm_notfull_index-1;
			insert_entry(vm, ppn, vpn, rw);
		}
		//else cold miss and memory full. make space and insert page.
		else if (swap_and_update(vm,ppn_hex,strategy,rw,verbose,access_num)==-1) return -1;
	}
	//else page is seen before. check for page fault.
	else if ((ipt[ppn]%16)==8 || (ipt[ppn]%16)==1 || (ipt[ppn]%16)==4 || (ipt[ppn]%16)==5) {
		b++;//increase miss count
		if (swap_and_update(vm,ppn_hex,strategy,rw,verbose,access_num)==-1) return -1;
	}
	//else the page is present in physical memory; page hit occurs.
	else {
		int vpn=ipt[ppn]/16;
		update_entry(vm, ppn, vpn, rw, strategy);
	}
	return 0;
}

int main(int argc, char const *argv[])
{
	srand(5635);
	bool verbose=false;
	if (argc==5) verbose=true;
	sscanf(argv[2],"%d",&num_frames);
	char* strategy=malloc(32);
	strcpy(strategy,argv[3]);
	char line[16];//length of line including \n
	int vm[num_frames];//physical memory, starting index is 0
	
	FILE *f;
	f=fopen(argv[1],"r");
	if (!f) {
		printf("trace file does not exist\n");
		return 0;
	}
	// making arrays with instructions and page numbers
	while (fgets(line, sizeof(line), f)) {
		ins[tot_accesses]=malloc(16);
		strcpy(ins[tot_accesses],line);
		char* token = strtok(line, " ");
		int length=strlen(token);
		char ppn_hex[length-2];
		strncpy(ppn_hex,token,length-3);
		ppn_hex[length-3]='\0';
		sscanf(ppn_hex,"%x",&nums[tot_accesses]);
		tot_accesses++;
	}
	
	// iterating through array for pagins simulation
	for (int i = 0; i < tot_accesses; i++) {
		a++;//increasing the number of memory accesses
		
		//parsing line to get address and read/write
		char* token = strtok(ins[i], " ");
		int length=strlen(token);
		char ppn_hex[length-2];
		strncpy(ppn_hex,token,length-3);
		ppn_hex[length-3]='\0';
		char rw = strtok(NULL, "\n")[1];
		
		// it strategy is OPT, access number can speed up the search 
		if (!strcmp(strategy,"OPT"))
			access_page(vm, ppn_hex, rw, strategy, verbose,i);
		
		// otherwise dummy number=0 is given 
		else
			access_page(vm, ppn_hex, rw, strategy, verbose,0);
		
	}
	print_results();
    fclose(f);
	
	return 0;
}