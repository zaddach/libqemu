#ifndef _CPU_ARCH_STRUCT_INFO_H
#define _CPU_ARCH_STRUCT_INFO_H
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/DebugInfo.h"

#include <unordered_map>

namespace llvm {
    class DICompositeType;
}

class StructInfo
{
public:
    StructInfo(llvm::Module *module, llvm::StructType *structType, llvm::DICompositeType *diStructType = nullptr, llvm::DITypeIdentifierMap *typeIdentifierMap = nullptr);
    bool findMember(unsigned offset, llvm::SmallVectorImpl<unsigned>& indices);
    bool findMember(llvm::StringRef name, llvm::SmallVectorImpl<unsigned>& indices);
    std::string getMemberName(llvm::ArrayRef<unsigned> indices);
    llvm::Type *getMemberType(llvm::ArrayRef<unsigned> indices);
    std::string getName();
    llvm::Module *getModule() {return m_module;}

private:
    llvm::Module *m_module;
    llvm::DataLayout m_dataLayout;
    llvm::StructType *m_structType;
    llvm::DICompositeType *m_debugInfoStructType;
    llvm::DITypeIdentifierMap *m_typeIdentifierMap;
};



  struct S2ECpuArchStructInfoPass : public llvm::ModulePass {
    static char ID;

    S2ECpuArchStructInfoPass();

    virtual bool runOnModule(llvm::Module &mod);
	virtual void getAnalysisUsage(llvm::AnalysisUsage& usage) const;

	bool findMember(unsigned offset, llvm::SmallVectorImpl<unsigned>& indices);

	/**
	 * Currently only finds direct members.
	 * (subelements like arrays are not supported)
	 */
	bool findMember(llvm::StringRef name, llvm::SmallVectorImpl<unsigned>& indices);

	llvm::Type *getMemberType(llvm::ArrayRef<unsigned> indices) {
	    assert(m_cpuArchStructInfo);
	    return m_cpuArchStructInfo->getMemberType(indices);
	}

	std::string getMemberName(llvm::ArrayRef<unsigned> indices) {
	    assert(m_cpuArchStructInfo);
	    return m_cpuArchStructInfo->getMemberName(indices);
	}

	std::string getStructName()  {
	    assert(m_cpuArchStructInfo);
	    return m_cpuArchStructInfo->getName();
	}
  private:
	StructInfo *m_cpuArchStructInfo;
	std::unordered_map<std::string, llvm::SmallVector<unsigned, 10> > m_cachedNameLookups;
	std::unordered_map<unsigned, llvm::SmallVector<unsigned, 10> > m_cachedOffsetLookups;

  };

#endif /* _CPU_ARCH_STRUCT_INFO_H */
