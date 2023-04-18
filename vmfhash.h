#ifndef VMFHASH_H_
#define VMFHASH_H_

#define VMF_HASHEMPTY 0xFFFFFFFF

unsigned long long vmf_hashstring64(char *string, unsigned int len){
	const unsigned long long prime = 1099511628211;
	unsigned long long hash = 14695981039346656037UL;

	for(unsigned int i = 0; i < len; ++i){
		hash ^= (unsigned long long)string[i];
		hash *= prime;
	}

	return hash;
}

typedef struct{
	unsigned long long hash;
	unsigned int dataoffset;
} _vmfhashentry;

typedef struct{
	_vmfhashentry *table;
	unsigned int size;
	unsigned int ressize;
	unsigned int datasize;
	unsigned int dataressize;
	char *data;
} _vmfhashtable;

void vmf_inithashtable(_vmfhashtable *table){
	table->dataressize = 1024;
	table->datasize = 0;
	table->data = malloc(table->dataressize);
	table->ressize = 512;
	table->size = 0;
	table->table = malloc(table->ressize * sizeof(_vmfhashentry));
	memset(table->table, VMF_HASHEMPTY, sizeof(_vmfhashentry) * table->ressize);
}

void vmf_growhashtable(_vmfhashtable *table, unsigned int increment){
	//table->table = realloc(table->table, (table->ressize + increment) * sizeof(_vmfhashentry));
	//memset(&table->table[table->ressize - 1], VMF_HASHEMPTY, increment * sizeof(_vmfhashentry));
	//table->ressize += increment;

	unsigned int newcapacity = table->ressize + increment;
	_vmfhashentry *newtable = malloc(newcapacity * sizeof(_vmfhashentry));
	memset(newtable, VMF_HASHEMPTY, newcapacity * sizeof(_vmfhashentry));

	for(unsigned int i = 0; i < table->size; ++i){
		if(table->table[i].dataoffset == VMF_HASHEMPTY) continue;

		//unsigned long long newhash =
	}
}

unsigned int vmf_insertstring(_vmfhashtable *table, char *string, unsigned int length){
	unsigned long long hash = vmf_hashstring64(string, length);
	unsigned int index = hash % (unsigned long long)table->ressize;
	char found = table->table[index].hash == hash;

	unsigned int i = 0;
	while(!found && table->table[index].dataoffset != VMF_HASHEMPTY){
		if(table->table[index].hash == hash){
			found = 1;
			break;
		}

		index = (index + (i*i)) % (unsigned long long)table->ressize;
		++i;
	}

	if(found){
		return table->table[index].dataoffset;
	}

	if(table->datasize + length + 1 >= table->dataressize){ //grow data
		table->dataressize *= 2;
		table->dataressize += length + 1;
		table->data = realloc(table->data, table->dataressize);
	}

	if(table->size >= table->ressize / 2){
		vmf_growhashtable(table, table->ressize); //grow to ressize * 2
	}

	table->table[index] = (_vmfhashentry){.hash = hash, .dataoffset = table->datasize};

	memcpy(&table->data[table->datasize], string, length);
	table->data[table->datasize + length] = '\n';
	table->datasize += length + 1;

	++(table->size);

	return table->table[index].dataoffset;
}


#endif
