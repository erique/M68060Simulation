
#include "M68060PairabilityTests.h"
#include "../Assert.h"

const char* PairabilityTestResultToString(PairabilityTestResult pairabilityTestResult)
{
	static const char* pairabilityTestResultStrings[] =
	{
		"Success",
		"Test2Failure_FirstInstructionIs_pOEPOnly",
		"Test2Failure_SecondInstructionIsNot_pOEPOrsOEP",
		"Test3Failure_SecondInstructionUsesPCRelativeAddressing",
		"Test4Failure_BothInstructionsReferenceMemory",
		"Test5Failure_SecondInstructionBaseRegisterDependsOnFirstInstructionAguResult",
		"Test5Failure_SecondInstructionBaseRegisterDependsOnFirstInstructionIeeResult",
		"Test5Failure_SecondInstructionIndexRegisterDependsOnFirstInstructionAguResult",
		"Test5Failure_SecondInstructionIndexRegisterDependsOnFirstInstructionIeeResult",
		"Test6Failure_SecondInstructionIeeARegisterDependsOnFirstInstructionAguResult",
		"Test6Failure_SecondInstructionIeeARegisterDependsOnFirstInstructionIeeResult",
		"Test6Failure_SecondInstructionIeeBRegisterDependsOnFirstInstructionAguResult",
		"Test6Failure_SecondInstructionIeeBRegisterDependsOnFirstInstructionIeeResult",
	};

	M68060_ASSERT((size_t) pairabilityTestResult < (sizeof pairabilityTestResultStrings / sizeof pairabilityTestResultStrings[0]), "Invalid pairabilityTestResult");
	
	return pairabilityTestResultStrings[(int) pairabilityTestResult];
}

PairabilityTestResult checkPairability_Test2_InstructionClassification(UOp* UOp0, UOp* UOp1)
{
	if (UOp0->pairability == Pairability_pOEP_Only)
		return PairabilityTestResult_Test2Failure_FirstInstructionIs_pOEPOnly;

	if (UOp1->pairability != Pairability_pOEP_Or_sOEP)
		return PairabilityTestResult_Test2Failure_SecondInstructionIsNot_pOEPOrsOEP;
		
	return PairabilityTestResult_Success;
}

PairabilityTestResult checkPairability_Test3_AllowableAddressingModesInsOEP(UOp* UOp1)
{
	if (UOp1->aguBase == ExecutionResource_PC)
		return PairabilityTestResult_Test3Failure_SecondInstructionUsesPCRelativeAddressing;

	// No need to test for Ops with double-indirection in sOEP, beecause those will generate pOEP-only support UOps
	
	return PairabilityTestResult_Success;
}

PairabilityTestResult checkPairability_Test4_AllowableOperandDataMemoryReference(UOp* UOp0, UOp* UOp1)
{
	bool UOp0HasMemoryReference = (UOp0->memoryRead || UOp0->memoryWrite);
	bool UOp1HasMemoryReference = (UOp1->memoryRead || UOp1->memoryWrite);

	if (UOp0HasMemoryReference && UOp1HasMemoryReference)
		return PairabilityTestResult_Test4Failure_BothInstructionsReferenceMemory;

	return PairabilityTestResult_Success;
}

PairabilityTestResult checkPairability_Test5_RegisterConflictsOnAguResources(UOp* UOp0, UOp* UOp1)
{
	if (isRegister(UOp1->aguBase))
	{
		if (UOp1->aguBase == UOp0->aguResult)
			return PairabilityTestResult_Test5Failure_SecondInstructionBaseRegisterDependsOnFirstInstructionAguResult;
		else if (UOp1->aguBase == UOp0->ieeResult)
			return PairabilityTestResult_Test5Failure_SecondInstructionBaseRegisterDependsOnFirstInstructionIeeResult;
	}
	if (isRegister(UOp1->aguIndex))
	{
		if (UOp1->aguIndex == UOp0->aguResult)
			return PairabilityTestResult_Test5Failure_SecondInstructionIndexRegisterDependsOnFirstInstructionAguResult;
		else if (UOp1->aguIndex == UOp0->ieeResult)
			return PairabilityTestResult_Test5Failure_SecondInstructionIndexRegisterDependsOnFirstInstructionIeeResult;
	}

	return PairabilityTestResult_Success;
}

PairabilityTestResult checkPairability_Test6_RegisterConflictsOnIeeResources(UOp* UOp0, UOp* UOp1)
{
/*
	Test6 Exceptions:

	1. If the primary OEP instruction is a simple “move long to register” (MOVE.L,Rx) and
	the destination register Rx is required as either the sOEP.A or sOEP.B input, the
	MC68060 bypasses the data as required and the test succeeds.

	2. In the following sequence of instructions:
		<op>.l,Dx
		mov.l Dx,<mem>
	the result of the pOEP instruction is needed as an input to the sOEP.IEE and the sOEP
	instruction is a move instruction. The destination operand for the memory write is sourced
	directly from the pOEP execute result and the test succeeds.
*/
	bool exception1 =
		(UOp0->ieeOperation == IeeOperation_Move) &&
		(UOp0->ieeOperationSize == OperationSize_Long) &&
		(
			isRegister(UOp1->ieeA) && UOp1->ieeA == UOp0->ieeResult ||
			isRegister(UOp1->ieeB) && UOp1->ieeB == UOp0->ieeResult
		);

	bool exception2 =
		(UOp0->ieeOperationSize == OperationSize_Long) &&
		(UOp1->ieeOperation == IeeOperation_Move) &&
		(UOp1->ieeOperationSize == OperationSize_Long) &&
		(UOp1->ieeResult == ExecutionResource_MemoryOperand) &&
		(ExecutionResource_D0 <= UOp1->ieeA && UOp1->ieeA <= ExecutionResource_D7) &&
		(UOp1->ieeA == UOp0->ieeResult);

	if (exception1 || exception2)
		return PairabilityTestResult_Success;

	if (isRegister(UOp1->ieeA))
	{
		if (UOp1->ieeA == UOp0->aguResult)
			return PairabilityTestResult_Test6Failure_SecondInstructionIeeARegisterDependsOnFirstInstructionAguResult;
		else if (UOp1->ieeA == UOp0->ieeResult)
			return PairabilityTestResult_Test6Failure_SecondInstructionIeeARegisterDependsOnFirstInstructionIeeResult;
	}
	if (isRegister(UOp1->ieeB))
	{
		if (UOp1->ieeB == UOp0->aguResult)
			return PairabilityTestResult_Test6Failure_SecondInstructionIeeBRegisterDependsOnFirstInstructionAguResult;
		else if (UOp1->ieeB == UOp0->ieeResult)
			return PairabilityTestResult_Test6Failure_SecondInstructionIeeBRegisterDependsOnFirstInstructionIeeResult;
	}

	return PairabilityTestResult_Success;
}

PairabilityTestResult checkPairability(UOp* UOp0, UOp* UOp1)
{
	PairabilityTestResult test2Result = checkPairability_Test2_InstructionClassification(UOp0, UOp1);
	PairabilityTestResult test3Result = checkPairability_Test3_AllowableAddressingModesInsOEP(UOp1);
	PairabilityTestResult test4Result = checkPairability_Test4_AllowableOperandDataMemoryReference(UOp0, UOp1);
	PairabilityTestResult test5Result = checkPairability_Test5_RegisterConflictsOnAguResources(UOp0, UOp1);
	PairabilityTestResult test6Result = checkPairability_Test6_RegisterConflictsOnIeeResources(UOp0, UOp1);

	if (test2Result != PairabilityTestResult_Success)
		return test2Result;

	if (test3Result != PairabilityTestResult_Success)
		return test3Result;

	if (test4Result != PairabilityTestResult_Success)
		return test4Result;

	if (test5Result != PairabilityTestResult_Success)
		return test5Result;

	if (test6Result != PairabilityTestResult_Success)
		return test6Result;

	return PairabilityTestResult_Success;
}
