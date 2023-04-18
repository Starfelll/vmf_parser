#include <stdlib.h>
#include <string.h>

#include "vmf.h"

#define VMFTOK_KEYVALUE 0
#define VMFTOK_BRACEOPEN 1
#define VMFTOK_BRACECLOSE 2
#define VMFTOK_CLASS 3
#define VMFTOK_EOF 4

typedef struct{
	unsigned int offset;
	unsigned short type;
	unsigned short length;
} _vmftoken;

#define VMFSTATE_CLEAR 0
#define VMFSTATE_KEYVALUE 1

#define VMFERR_OK 0
#define VMFERR_KEYVALUE 1
#define VMFERR_NOTCOMPLETE 2

typedef struct{
	vmfnode **stack;
	const char *vmfcontent;
	unsigned int stacksize;
	unsigned int stackres;
	unsigned int state;
} _vmfstack;

typedef struct{
	char *buffer;
	unsigned int buffsize;
	unsigned int buffres;
} _vmfbuffer;

//init the recursive iterator info struct
vmfrecursive_iterator vmf_reciter_init(vmfnode *node){
	vmfrecursive_iterator reciter;
	reciter.stackres = 16;
	reciter.stack = malloc(reciter.stackres * sizeof(_vmfrec));
	reciter.stacksize = 1;

	reciter.stack[0] = (_vmfrec){.node = node, .curchild = 0, .visited = 0};

	return reciter;
}

//reuse the recursive iterator info struct
void vmf_reciter_reuse(vmfrecursive_iterator *info, vmfnode *node){
	info->stacksize = 1;
	info->stack[0] = (_vmfrec){.node = node, .curchild = 0, .visited = 0};
}

void vmf_reciter_free(vmfrecursive_iterator *info){
	free(info->stack);
	info->stacksize = 0;
	info->stackres = 0;
}

//get next node, for iterating over the tree structure
vmfnode *vmf_reciter_next(vmfrecursive_iterator *info){
	while(1){
		if(info->stacksize < 1){
			return VMF_NULL;
		}

		_vmfrec *stacktop = &info->stack[info->stacksize - 1];
		if(stacktop->visited){
			//if no more children (or no children at all)
			if(stacktop->curchild >= stacktop->node->numchildren){
				--info->stacksize; //pop off the stack
				continue;
			}

			++stacktop->curchild;

			if(info->stacksize + 1 >= info->stackres){
				info->stackres += info->stackres + 1;
				info->stack = realloc(info->stack, info->stackres * sizeof(_vmfrec));
			}
			//push the next child onto a stack
			info->stack[info->stacksize] = (_vmfrec){
				.node = &stacktop->node->children[stacktop->curchild - 1], .curchild = 0, .visited = 0
			};
			++info->stacksize;
		}

		break;
	}

	info->stack[info->stacksize - 1].visited = 1;
	return info->stack[info->stacksize - 1].node;
}

//intializes a node in given place
void vmf_initnode(vmfnode *node){
	node->numchildren = 0;
	node->reschildren = 4;
	node->children = malloc(4 * sizeof(vmfnode));
	node->numkvalues = 0;
	node->reskvalues = 8;
	node->keyvalues = malloc(8 * sizeof(vmfkeyvalue));
}

//appends a child into the node
void vmf_addchild(vmfnode *parent, vmfnode *child){
	if(parent->reschildren < 1){
		parent->reschildren = parent->numchildren > 0xFFFF ? 0xFFFF : parent->numchildren + 1; //double the capacity
		parent->children = realloc(parent->children, (parent->numchildren + parent->reschildren) * sizeof(vmfnode));
	}

	parent->children[parent->numchildren] = *child;
	++parent->numchildren;
	--parent->reschildren;
}

//appends a keyvalue into the node
void vmf_addkeyvalue(vmfnode *node, vmfkeyvalue kv){
	if(node->reskvalues < 1){
		node->reskvalues = node->numkvalues + 1; //double the capacity
		node->keyvalues = realloc(node->keyvalues, (node->numkvalues + node->reskvalues) * sizeof(vmfkeyvalue));
	}

	node->keyvalues[node->numkvalues] = kv;
	++node->numkvalues;
	--node->reskvalues;
}

//initializes and returns a pointer to a next child
vmfnode *vmf_getnextchild(vmfnode *parent){
	if(parent->reschildren < 1){
		parent->reschildren = parent->numchildren > 0xFFFF ? 0xFFFF : parent->numchildren + 1; //double the capacity
		parent->children = realloc(parent->children, (parent->numchildren + parent->reschildren) * sizeof(vmfnode));
	}

	vmf_initnode(&parent->children[parent->numchildren]);

	--parent->reschildren;

	return &parent->children[parent->numchildren++];
}

void vmf_freenode_reciter(vmfnode *node, vmfrecursive_iterator *info){
	vmf_reciter_reuse(info, node);

	while(info->stacksize > 0){
		_vmfrec *stacktop = &info->stack[info->stacksize - 1];

		//already looped through each children or there are no children at all
		if(stacktop->node->numchildren <= stacktop->curchild + 1){
			//leaf, free everything
			for(unsigned int i = 0; i < stacktop->node->numkvalues; ++i){
				free(stacktop->node->keyvalues[i].key);
				free(stacktop->node->keyvalues[i].value);
			}
			free(stacktop->node->children);
			free(stacktop->node->keyvalues);
			free(stacktop->node->classname);
			*stacktop->node = (vmfnode){0};

			--info->stacksize;
			continue;
		}

		if(info->stacksize + 1 >= info->stackres){
			info->stackres += 1 + info->stackres;
			info->stack = realloc(info->stack, info->stackres * sizeof(_vmfrec));
		}

		//push the next child onto a stack
		info->stack[info->stacksize] = (_vmfrec){
			.node = &stacktop->node->children[stacktop->curchild],
			.curchild = 0,
			.visited = 0
		};
		++info->stacksize;
		++stacktop->curchild;
	}
}

void vmf_freenode(vmfnode *node){
	vmfrecursive_iterator info = vmf_reciter_init(node);
	vmf_freenode_reciter(node, &info);
	vmf_reciter_free(&info);
}

void vmf_free(vmfroot *root){
	vmf_freenode(&root->root);
	*root = (vmfroot){0};
}

char vmf_ispropername(char c){
	return (c > 64 && c < 91) || (c > 96 && c < 123) || c == '_' || c == '-' || (c > 47 && c < 58);
}

unsigned long long vmf_hashstring64(const char *string, unsigned int len){
	const unsigned long long prime = 1099511628211;
	unsigned long long hash = 14695981039346656037UL;

	for(unsigned int i = 0; i < len; ++i){
		hash ^= (unsigned long long)string[i];
		hash *= prime;
	}

	return hash;
}

unsigned int vmf_hashstring32(const char *str, unsigned int len){
	const unsigned int prime = 16777619;
	unsigned int hash = 2166136261;

	for (unsigned int i = 0; i < len; ++i) {
		hash ^= (unsigned int)str[i];
		hash *= prime;
	}

	return hash;
}

char *vmf_insertstring(const char *str, unsigned int len){
	char *string = malloc(len + 1);
	memcpy(string, str, len);
	string[len] = '\0';

	return string;
}

unsigned int vmf_emittoken(_vmfstack *stack, _vmftoken *token){
	if(stack->state == VMFSTATE_KEYVALUE && token->type != VMFTOK_KEYVALUE){
		return VMFERR_KEYVALUE;
	}

	vmfnode *stacktop = stack->stack[stack->stacksize - 1];

	switch(token->type){
	case VMFTOK_CLASS:{
		vmfnode *child = vmf_getnextchild(stacktop);
		child->classname = vmf_insertstring(stack->vmfcontent + token->offset, token->length);

		if(stack->stacksize + 1 >= stack->stackres){
			stack->stackres += stack->stackres + 1;
			stack->stack = realloc(stack->stack, stack->stackres * sizeof(vmfnode*));
		}

		stack->stack[stack->stacksize] = child;
		++stack->stacksize;
	}break;
	case VMFTOK_KEYVALUE:{
		if(stack->state == VMFSTATE_CLEAR){
			stack->state = VMFSTATE_KEYVALUE; //handle the first half of keyvalue, that's the key
			//set the state and wait for the value part. If value parts does not come throw an error tantrum
			vmfkeyvalue kv;
			kv.key = vmf_insertstring(stack->vmfcontent + token->offset, token->length);
			vmf_addkeyvalue(stacktop, kv);
		} else if(stack->state == VMFSTATE_KEYVALUE){
			//handle value, parse it or something
			stack->state = VMFSTATE_CLEAR;

			vmfkeyvalue *lastkv = &stacktop->keyvalues[stacktop->numkvalues - 1];
			lastkv->value = vmf_insertstring(stack->vmfcontent + token->offset, token->length);
			//lastkv->datatype = VMFDTYPE_STRING; //leave it as string, let others parse it once they know the context
			/*
			 * no type needed. I'll leave everything as a string. Parsin individual structures like side,
			 * solid is left for the programmer
			 */
		}
	}break;
	case VMFTOK_BRACECLOSE:
		--stack->stacksize; //pop one off the stack
		//shrink the memory, most of the nodes won't be touched any time soon
		stacktop->children = realloc(stacktop->children, stacktop->numchildren * sizeof(vmfnode));
		stacktop->keyvalues = realloc(stacktop->keyvalues, stacktop->numkvalues * sizeof(vmfkeyvalue));
		stacktop->reschildren = 0;
		stacktop->reskvalues = 0;
		break;
	case VMFTOK_EOF:
		if(stack->stacksize > 1){
			//if we still have something else than the root on stack - ERROR ERROR
			return VMFERR_NOTCOMPLETE;
		}
		break;
	}

	return VMFERR_OK;
}

vmfroot string2vmf(const char *vmfstr){
	unsigned int i = 0;
	unsigned char not_eof = 1;

	vmfroot vmf;
	vmfnode top;
	/*
	 * top node (root) is initialized in a bit different way.
	 * We don't need keyvalues, we need a lot of space for children
	 */
	top.reschildren = 128;
	top.numchildren = 0;
	top.children = malloc(sizeof(vmfnode) * top.reschildren);
	top.numkvalues = 0;
	top.reskvalues = 0;
	top.classname = 0;

	_vmfstack stack;
	stack.stackres = 8;
	stack.stacksize = 1;
	stack.stack = malloc(stack.stackres * sizeof(vmfnode*));
	stack.state = VMFSTATE_CLEAR;
	stack.vmfcontent = vmfstr;
	stack.stack[0] = &top;

	while(not_eof){
		_vmftoken curtok;
		switch(vmfstr[i]){
		case '}':
			curtok.type = VMFTOK_BRACECLOSE;
			curtok.length = 1;
			curtok.offset = i;
			++i;
			break;
		case '"':
			curtok.type = VMFTOK_KEYVALUE;
			++i; //set the index to first char after the quotemark
			unsigned int j = 0;
			while(vmfstr[i + j] != '"' || !vmfstr[i+j]){
				++j; //loop until EOF or second quotemark
			}
			curtok.offset = i;
			curtok.length = j;
			i += j + 1; //j+1 = length of text, add one to skip the quotemark
			break;
		case '{': //why don't we care about open braces? Because they only exist after the class name, so they're redundant
		case '\r':
		case '\n':
		case '\t':
		case ' ':
			++i;
			continue; //dont care about spaces, skip the token emission. Lowering the emission
		case '\0':
			not_eof = 0; //eof reached, so not_eof is true xD all of that just to avoid one NOT instruction
			curtok.type = VMFTOK_EOF;
			break;
		default:
			//does not start with '"' so it must be a class name
			if(vmf_ispropername(vmfstr[i])){
				curtok.type = VMFTOK_CLASS;
				unsigned int j = 1; //skip the first letter, no need to check twice
				while(vmf_ispropername(vmfstr[i + j])){
					++j; //loop until !letter
				}
				curtok.offset = i;
				curtok.length = j;
				i += j;
			} else {
				++i;
				continue; //other weird characters
			}
		}

		//emit token
		vmf_emittoken(&stack, &curtok);
	}

	/*printf("Data size: %f kB\nLookup size: %f kB\nTotal size: %f kB\n", vmf.data.datasize * 0.001, vmf.data.lookupsize * sizeof(_vmflookup) * 0.001,
			(vmf.data.datasize + vmf.data.lookupsize * sizeof(_vmflookup)) * 0.001);
	printf("Reserved Data: %f kB", vmf.data.dataressize * 0.001);*/
	vmf.root = top;
	free(stack.stack);
	return vmf;
}

void _vmfensuresize(_vmfbuffer *buffer, unsigned int addsize){
	if(buffer->buffsize + addsize >= buffer->buffres){
		buffer->buffres = buffer->buffres + addsize;
		buffer->buffer = realloc(buffer->buffer, buffer->buffres);
	}
}

unsigned int _vmfstrlen(const char *str){
	unsigned int i = 0;
	for(;str[i] != '\0'; ++i);

	return i;
}

void _vmfstrinserttabs(_vmfbuffer *buff, unsigned int numtabs){
	for(unsigned int j = 0; j < numtabs; ++j){
		buff->buffer[buff->buffsize] = '\t';
		++buff->buffsize; //tabs
	}
}

unsigned int vmf2string(vmfroot *vmf, char **out){
	_vmfbuffer buffer;
	buffer.buffsize = 0;
	buffer.buffres = 8192;
	buffer.buffer = malloc(buffer.buffres);

	unsigned int stacksize;
	unsigned int resstack = 16;
	_vmfrec *stack = malloc(sizeof(_vmfrec) * resstack);

	for(unsigned int i = 0; i < vmf->root.numchildren; ++i){
		stacksize = 1;
		stack[0] = (_vmfrec){.node = &vmf->root.children[i], .curchild = 0, .visited = 0};

		while(stacksize > 0){
			_vmfrec *stacktop = &stack[stacksize - 1];

			/*if(stacktop->node->numchildren < 1 && stacktop->node->numkvalues < 1){
				//empty node, probably has been deleted, pop and continue
				--stacksize;
				continue;
			}*/

			if(!stacktop->visited){
				unsigned int classnamelen = _vmfstrlen(stacktop->node->classname);
				//2 * tabs + classnamelen + \r\n{\r\n;
				_vmfensuresize(&buffer, (stacksize - 1) * 2 + classnamelen + 5);
				_vmfstrinserttabs(&buffer, stacksize - 1);
				memcpy(buffer.buffer + buffer.buffsize, stacktop->node->classname, classnamelen);
				buffer.buffsize += classnamelen;
				buffer.buffer[buffer.buffsize++] = '\r';
				buffer.buffer[buffer.buffsize++] = '\n';
				_vmfstrinserttabs(&buffer, stacksize - 1);
				buffer.buffer[buffer.buffsize++] = '{';
				buffer.buffer[buffer.buffsize++] = '\r';
				buffer.buffer[buffer.buffsize++] = '\n';

				for(unsigned int j = 0; j < stacktop->node->numkvalues; ++j){
					vmfkeyvalue kv = stacktop->node->keyvalues[j];

					unsigned int keylen = _vmfstrlen(kv.key);
					unsigned int vallen = _vmfstrlen(kv.value);
					//tabs + tab + keylen + vallen + 4*quotes + space + newline + carriage
					_vmfensuresize(&buffer, (stacksize - 1) + keylen + vallen + 8);
					//tabs + tab
					_vmfstrinserttabs(&buffer, stacksize);
					buffer.buffer[buffer.buffsize++] = '"';
					memcpy(buffer.buffer + buffer.buffsize, kv.key, keylen);
					buffer.buffsize += keylen;
					buffer.buffer[buffer.buffsize++] = '"';
					buffer.buffer[buffer.buffsize++] = ' ';
					buffer.buffer[buffer.buffsize++] = '"';
					memcpy(buffer.buffer + buffer.buffsize, kv.value, vallen);
					buffer.buffsize += vallen;
					buffer.buffer[buffer.buffsize++] = '"';
					buffer.buffer[buffer.buffsize++] = '\r';
					buffer.buffer[buffer.buffsize++] = '\n';
				}
				stacktop->visited = 1;
			}

			if(stacktop->curchild >= stacktop->node->numchildren){
				//tabs + bracket + carriage + newline
				_vmfensuresize(&buffer, (stacksize - 1) + 3);
				_vmfstrinserttabs(&buffer, stacksize - 1);
				buffer.buffer[buffer.buffsize++] = '}';
				buffer.buffer[buffer.buffsize++] = '\r';
				buffer.buffer[buffer.buffsize++] = '\n';
				--stacksize;
				continue;
			}

			if(stacksize + 1 >= resstack){
				resstack += resstack + 1;
				stack = realloc(stack, resstack * sizeof(_vmfrec));
			}

			stack[stacksize] = (_vmfrec){.node = &stacktop->node->children[stacktop->curchild], .curchild = 0, .visited = 0};
			++stacktop->curchild;
			++stacksize;
		}
	}

	buffer.buffer = realloc(buffer.buffer, buffer.buffsize + 1);
	buffer.buffer[buffer.buffsize] = '\0'; //do not count the null terminator into a size, useful for saving files

	free(stack);

	*out = buffer.buffer;
	return buffer.buffsize;
}
