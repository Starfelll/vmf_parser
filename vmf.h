#ifndef VMF_H_
#define VMF_H_

#define VMF_NULL 0

#define VMFDTYPE_INT 1
#define VMFDTYPE_FLOAT 2
#define VMFDTYPE_STRING 3
#define VMFDTYPE_BOOL 4
#define VMFDTYPE_VECTOR 5
#define VMFDTYPE_COLOR 6

typedef struct{
	char *key;
	char *value;
	//unsigned char datatype;
} vmfkeyvalue;

struct _vmfnode;
struct _vmfroot;

typedef struct _vmfnode{
	struct _vmfnode *children;
	vmfkeyvalue *keyvalues;
	char *classname;
	unsigned int numchildren;
	unsigned short numkvalues;
	unsigned short reschildren;
	unsigned short reskvalues;
} vmfnode;

typedef struct{
	/*vmfnode *children;
	unsigned int numchildren;
	unsigned int reschildren;*/
	vmfnode root;
} vmfroot;

typedef struct{
	vmfnode *node;
	unsigned short curchild;
	unsigned char visited;
} _vmfrec; //used for pseudo-recursion

typedef struct{
	_vmfrec *stack;
	unsigned int stacksize;
	unsigned int stackres;
} vmfrecursive_iterator; //recursive iterator info

//converts a null-terminated string into a vmf structure
vmfroot string2vmf(const char *vmfstr);
//converts a vmf structure into a null-terminated string
unsigned int vmf2string(vmfroot *vmf, char **out);

//init the recursive iterator info struct
vmfrecursive_iterator vmf_reciter_init(vmfnode *node);
//reuse the recursive iterator info struct
void vmf_reciter_reuse(vmfrecursive_iterator *info, vmfnode *node);
void vmf_reciter_free(vmfrecursive_iterator *info);
//get next node, for iterating over the tree structure
vmfnode *vmf_reciter_next(vmfrecursive_iterator *info);

//intializes a node in given place
void vmf_initnode(vmfnode *node);
/*
 * appends a child into the node
 * use vmf_getnextchild to avoid copying
 */
void vmf_addchild(vmfnode *parent, vmfnode *child);
//appends a keyvalue into the node
void vmf_addkeyvalue(vmfnode *node, vmfkeyvalue kv);
//initializes and returns a pointer to a next child
vmfnode *vmf_getnextchild(vmfnode *parent);

//recursive free
void vmf_freenode(vmfnode *node);
//free node with recursive iterator already supplied, to avoid malloc. Good for removing multiple nodes at a time
void vmf_freenode_reciter(vmfnode *node, vmfrecursive_iterator *info);
//frees the entire vmf structure, with data, lookup tables etc.
void vmf_free(vmfroot *root);

#endif
