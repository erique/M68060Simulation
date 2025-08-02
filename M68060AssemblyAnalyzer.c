#include "M68060InstructionDecoder/M68060DecodeOpIntoUOps.h"
#include "M68060InstructionDecoder/M68060PairabilityTests.h"
#include "M68060InstructionDecoder/M68060InstructionLengthDecoder.h"
#include "M68060InstructionDecoder/M68060OpWordDecodeInformation.h"
#include "Musashi/m68kcpu.h"
#include "Types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


#define MAX_INSTRUCTIONS 1000
#define MAX_INSTRUCTION_WORDS 8

typedef struct
{
    uint32_t offset;
	uint16_t instructionWords[MAX_INSTRUCTION_WORDS];
	uint numWords;
	UOp UOps[16];
	uint numUOps;
	bool isValid;
	char disassembly[256];
} InstructionAnalysis;


bool parseInstructionFromBinary(const uint8_t* data, size_t offset, size_t dataSize, InstructionAnalysis* analysis)
{
	if (offset + 1 >= dataSize)
		return false;

	uint16_t instructionWords[MAX_INSTRUCTION_WORDS];
	uint numWordsAvailable = 0;

	for (uint i = 0; i < MAX_INSTRUCTION_WORDS && offset + (i * 2) + 1 < dataSize; i++)
	{
		instructionWords[i] = (data[offset + (i * 2)] << 8) | data[offset + (i * 2) + 1];
		numWordsAvailable++;
	}

	if (numWordsAvailable == 0)
		return false;

	InstructionLength instructionLength;
	if (!decodeInstructionLengthFromInstructionWords(instructionWords, numWordsAvailable, &instructionLength))
	{
		analysis->instructionWords[0] = instructionWords[0];
		analysis->numWords = 1;
	}
	else
	{
		analysis->numWords = instructionLength.totalWords;
		if (analysis->numWords > MAX_INSTRUCTION_WORDS)
			analysis->numWords = MAX_INSTRUCTION_WORDS;
		for (uint i = 0; i < analysis->numWords; i++)
			analysis->instructionWords[i] = instructionWords[i];
	}

	analysis->offset = offset;
	return true;
}

bool analyzeInstruction(InstructionAnalysis* analysis)
{
    for (uint word = 0; word < analysis->numWords; ++word)
        m68k_write_memory_16(word * sizeof(uint16_t), analysis->instructionWords[word]);

    char musashiDisassembly[256];
    m68k_disassemble(musashiDisassembly, 0, M68K_CPU_TYPE_68040);

    snprintf(analysis->disassembly, sizeof(analysis->disassembly), "%s", musashiDisassembly);

	if (!decomposeOpIntoUOps(analysis->instructionWords, analysis->numWords, analysis->UOps, &analysis->numUOps))
	{
		printf("Warning: Failed to decode instruction: '%s' (opcode: ", analysis->disassembly);
		for (uint i = 0; i < analysis->numWords; i++)
			printf("%04x ", analysis->instructionWords[i]);
		printf(") @ 0x%x\n", analysis->offset);
		return false;
	}

	analysis->isValid = true;
	return true;
}

const char* getPairabilityString(Pairability pairability)
{
    return PairabilityToString(pairability);
}

void printInstructionAnalysis(const InstructionAnalysis* analysis, int index)
{
	char hexStr[32] = "";
	for (uint i = 0; i < analysis->numWords; i++)
	{
		char temp[8];
		sprintf(temp, "%04x", analysis->instructionWords[i]);
		if (i > 0) strcat(hexStr, " ");
		strcat(hexStr, temp);
	}

    const OpWordDecodeInfo* opWordDecodeInfo = getOpWordDecodeInformation(analysis->instructionWords[0]);

	printf("  %2d: %-24s %-30s [UOps: %d, %s]\n", 
		index + 1,
		hexStr,
		analysis->disassembly,
		analysis->numUOps,
		PairabilityToString(opWordDecodeInfo->pairability));
		
	// for (uint i = 0; i < analysis->numUOps; i++)
	// {
	// 	const UOp* uop = &analysis->UOps[i];
	// 	printf("    UOp[%d]: ieeA=%d, ieeB=%d, ieeResult=%d, aguResult=%d, op=%d\n",
	// 		i, uop->ieeA, uop->ieeB, uop->ieeResult, uop->aguResult, uop->ieeOperation);
	// }
}

void printPairingAnalysis(const InstructionAnalysis* instructions, int numInstructions)
{
	printf("\nPairing Analysis:\n");

	int successfulPairs = 0;

	for (int i = 0; i < numInstructions - 1; i++)
	{
		if (!instructions[i].isValid || !instructions[i+1].isValid || 
			instructions[i].numUOps == 0 || instructions[i+1].numUOps == 0)
			continue;

		const UOp* firstUOp = &instructions[i].UOps[instructions[i].numUOps - 1];
		const UOp* secondUOp = &instructions[i+1].UOps[0];

		printf("  Pair %d-%d: ", i + 1, i + 2);
		// printf("UOp0[ieeResult=%d] vs UOp1[ieeA=%d, ieeB=%d] - ", 
		// 	firstUOp->ieeResult, secondUOp->ieeA, secondUOp->ieeB);

		PairabilityTestResult result = checkPairability((UOp*)firstUOp, (UOp*)secondUOp);

		if (result == PairabilityTestResult_Success)
		{
			printf("SUCCESS - Instructions can pair\n");
			successfulPairs++;
		}
		else
		{
			printf("FAILED - %s\n", PairabilityTestResultToString(result));
		}
	}

	printf("\nPairing Summary:\n");
	printf("  Successful pairs: %d\n", successfulPairs);
	printf("  Total adjacent pairs: %d\n", numInstructions - 1);
}

void printCycleAnalysis(const InstructionAnalysis* instructions, int numInstructions)
{
	printf("\nProjected Cycle Cost:\n");

	int totalInstructions = 0;
	int pairedExecutions = 0;
	int sequentialExecutions = 0;

	for (int i = 0; i < numInstructions; i++)
	{
		if (instructions[i].isValid)
			totalInstructions++;
	}

	for (int i = 0; i < numInstructions - 1; i++)
	{
		if (!instructions[i].isValid || !instructions[i+1].isValid ||
			instructions[i].numUOps == 0 || instructions[i+1].numUOps == 0)
		{
			sequentialExecutions++;
			continue;
		}

		const UOp* firstUOp = &instructions[i].UOps[instructions[i].numUOps - 1];
		const UOp* secondUOp = &instructions[i+1].UOps[0];

		PairabilityTestResult result = checkPairability((UOp*)firstUOp, (UOp*)secondUOp);

		if (result == PairabilityTestResult_Success)
		{
			pairedExecutions++;
			i++;
		}
		else
		{
			sequentialExecutions++;
		}
	}

	if (numInstructions > 0 && !pairedExecutions * 2 >= totalInstructions)
		sequentialExecutions++;

	int estimatedCycles = pairedExecutions + sequentialExecutions;
	int unpairedCycles = totalInstructions;

	printf("  Total Instructions: %d\n", totalInstructions);
	printf("  Paired Executions: %d\n", pairedExecutions);
	printf("  Sequential Executions: %d\n", sequentialExecutions);
	printf("  Estimated Cycles: %d (vs %d unpaired)\n", estimatedCycles, unpairedCycles);

	if (estimatedCycles < unpairedCycles)
	{
		printf("  Performance Gain: %.1f%% faster\n", 
			100.0 * (unpairedCycles - estimatedCycles) / unpairedCycles);
	}
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		printf("M68060 Binary Analyzer\n");
		printf("Analyzes M68K binary for instruction pairing and cycle costs\n");
		printf("Usage: %s <raw_binary_file>\n\n", argv[0]);
        printf("Typically created using VASM:\n");
        printf("vasmm68k_mot -m68060 -Fbin file.s -o file.bin\n");
		return 1;
	}

	FILE* file = fopen(argv[1], "rb");
	if (!file)
	{
		printf("Error: Unable to open file '%s'\n", argv[1]);
		return 1;
	}

	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (fileSize <= 0)
	{
		printf("Error: Empty or invalid file\n");
		fclose(file);
		return 1;
	}

	uint8_t* binaryData = malloc(fileSize);
	if (!binaryData)
	{
		printf("Error: Unable to allocate memory\n");
		fclose(file);
		return 1;
	}

	if (fread(binaryData, 1, fileSize, file) != (size_t)fileSize)
	{
		printf("Error: Unable to read file\n");
		free(binaryData);
		fclose(file);
		return 1;
	}
	fclose(file);

	printf("M68060 Binary Analysis for: %s (%ld bytes)\n\n", argv[1], fileSize);

	InstructionAnalysis instructions[MAX_INSTRUCTIONS];
	int numInstructions = 0;

	for (size_t offset = 0; offset < (size_t)fileSize && numInstructions < MAX_INSTRUCTIONS; )
	{
		if (parseInstructionFromBinary(binaryData, offset, fileSize, &instructions[numInstructions]))
		{
			if (analyzeInstruction(&instructions[numInstructions]))
			{
				numInstructions++;
				offset += instructions[numInstructions - 1].numWords * 2;
			}
			else
			{
				offset += instructions[numInstructions].numWords * 2;
			}
		}
		else
		{
			offset += 2;
		}
	}

	free(binaryData);

	if (numInstructions == 0)
	{
		printf("No valid instructions found in the file.\n");
		return 1;
	}

	printf("Instructions:\n");
	for (int i = 0; i < numInstructions; i++)
		printInstructionAnalysis(&instructions[i], i);

	printf("\nStarting pairing analysis...\n");
	fflush(stdout);

	printPairingAnalysis(instructions, numInstructions);

	printf("Starting cycle analysis...\n");
	fflush(stdout);

	printCycleAnalysis(instructions, numInstructions);

	printf("Analysis complete.\n");
	return 0;
}