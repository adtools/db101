int strlcmp(const char *, const char *, size_t);

typedef enum
{
	PPC_BRANCH,
	PPC_BRANCHCOND,
	PPC_BRANCHTOLINK,
	PPC_BRANCHTOLINKCOND,
	PPC_BRANCHTOCTR,
	PPC_BRANCHTOCTRCOND,
	PPC_OTHER
} ppctype;

ppctype PPC_DisassembleBranchInstr(uint32 instr, int32 *reladdr);

typedef enum
{
	NOBRANCH,
	NORMALBRANCH,
	NORMALBRANCHCOND,
	ALLOWEDBRANCH,
	ALLOWEDBRANCHCOND,
	DISALLOWEDBRANCH,
	DISALLOWEDBRANCHCOND
} branch;

branch is_branch_allowed(void);
BOOL try_import_segment(uint32);
struct stab_function *get_branched_function(void);
