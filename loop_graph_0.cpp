/*
 * DFGenTool is a Data Flow Graph (DFG) generation tool, which converts loops
 * in a sequential program given in high level language like C/C++ into a DFG.
 * This file generates DFGs with PHI node modification and GEP expansion.
 * For complete list of authors refer to AUTHORS.txt.
 * For more details about the license refer to LICENSE.txt.
 * ----------------------------------------------------------------------------
 * 
 * Copyright (C) 2012 Apala Guha
 * Copyright (C) 2016 Manideepa Mukherjee
 * 
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */


#include "llvm/Pass.h"
#include "llvm/Support/CFG.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include <map>
#include <list>
#include "llvm/Analysis/CallGraph.h"
#include <set>
#include "llvm/Support/raw_ostream.h"
#include "loop_graph_analysis.h"
#include "llvm/Analysis/PostDominators.h"
#include <assert.h>
#include "llvm/IR/Type.h"
#include <stdio.h>
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/ArrayRef.h"
 #include "llvm/ADT/SmallBitVector.h"

#define NINTERVALS (100)
#define PRINT_THRESH (0)
#define COV (1)
#define ENTRY_POINT(a) printf(" in %s \n", a);getchar();getchar();getchar();
#define stringify( name ) # name

using namespace llvm;
using namespace std;


const char* typeNames[] = {
"VoidTyID",
"HalfTyID",
"FloatTyID",
"DoubleTyID",
"X86_FP80TyID",
"FP128TyID",
"PPC_FP128TyID",
"LabelTyID",
"MetadataTyID",
"X86_MMXTyID",
"IntegerTyID",
"FunctionTyID",
"StructTyID",
"ArrayTyID",
"PointerTyID",
"VectorTyID",
"NumTypeIDs",
"LastPrimitiveTyID",
"FirstDerivedTyID"
};

map <unsigned int, clust_graph> graphs;

unsigned int nodeID = 0;
unsigned int edgeID = 0;

namespace {

	map <unsigned int, double> topLoops;


	unsigned int loopID = 0;

	class LoopGraphAnalysisPass_0 : public FunctionPass {

		public:

			static char ID; 		// Class identification, replacement for typeinfo.
			LoopInfo* LI;
			double totWt;
			double sumWt;
			double wt;


			explicit LoopGraphAnalysisPass_0() : FunctionPass(ID) {}


			void getAnalysisUsage(AnalysisUsage &AU) const {
				AU.setPreservesAll();
				AU.addRequired<PostDominatorTree>();
				AU.addRequired<LoopInfo>();
				AU.addRequired<DependenceAnalysis>();
				AU.addPreserved<DependenceAnalysis>();
				AU.addRequired<ScalarEvolution>();
				AU.addPreserved<ScalarEvolution>();
				AU.addRequired<AliasAnalysis>();
				AU.addPreserved<AliasAnalysis>();
				AU.addRequired<MemoryDependenceAnalysis>();
				AU.addPreserved<MemoryDependenceAnalysis>();
			}

			virtual bool doInitialization(Module& M) {	//doInitialization
				return false;
			}


			virtual bool runOnFunction(Function& F) {
				LI = &getAnalysis<LoopInfo>();
				for (LoopInfo::iterator I = LI->begin(), E = LI->end(); I != E; ++I)	//gives the collection of Loops
					ProcessLoop(*I);
				return false;
			}


			bool ProcessLoop(Loop* L) {
				for (Loop::iterator I = L->begin(), E = L->end(); I != E; ++I) {
					ProcessLoop(*I);
				}

				map <unsigned int, double>::iterator topLoopIter = topLoops.find(loopID);  //check if current loop is a top loop

				if(L->getSubLoops().size()==0) {	//check if current loop is the innermost loop
					PrintLoop(L);	//print loop
					if (!FormNodes(L,loopID)) {	//iterate over the loop instructions and form nodes for instructions
						printf("loop failed %.5lf\n", topLoopIter->second);
					}

					AddDataEdges(L, loopID);	//introduce data dependence edges into graph

					AddCtrlEdges(L, loopID);	//introduce control dependence edges into graph

					bool success = WriteLoopGraph(loopID, topLoopIter->second);

					if(success) {
						list<clust_node> *gepNodes = new list<clust_node>;
						RemoveGEP(gepNodes, loopID);
						PrintDotGraph (gepNodes, loopID);
					}
				}

				loopID ++;
				nodeID = 0; //reset
				edgeID = 0;

				return false;
			}

		private:

			void PrintLoop(Loop* L) {			//print the instructions in the loop
				for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; bi ++) {		//for each block
					BasicBlock* bbl = *bi;
					for (BasicBlock::const_iterator ins = bbl->begin(), ie = bbl->end(); ins != ie; ins++)	{	//for each instruction
						printf("\t%s\n", ins->getOpcodeName());
					}
				}
			}

			bool FormNodes(Loop* L, unsigned int id) {
				clust_graph newGraph;

				for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; bi ++) {
					BasicBlock* bbl = *bi;
					for (BasicBlock::iterator ins = bbl->begin(), ie = bbl->end(); ins != ie; ins++) {	//for each ins

						clust_node newNode;
						newNode.ins = &*ins;
						newNode.id = ++ nodeID ;
						newNode.entryNode = false;
						newNode.wt = wt;
						newNode.nodeType = INSTNODE;
						newNode.depth = 0;

						char t = 0;	//print type of instruction (V=vector, F=floating point, N=integer)

						//-- handle non-cast instructions
						if (!Instruction::isCast(((Instruction*)(&*ins))->getOpcode())) {
							Type* datTyp = (&*ins)->getType();

							switch (datTyp->getTypeID())
							{
								case Type::HalfTyID :
								case Type::FloatTyID 	:
								case Type::DoubleTyID 	:
								case Type::X86_FP80TyID 	:
								case Type::FP128TyID 	:
								case Type::PPC_FP128TyID 	:
									t = 'F';
									break;
								case Type::VectorTyID :
									t = 'V';
									break;
								default:
									t = 'N';
									break;
							};
						}

						else {	// handle cast instructions
							if (((CastInst*)(&*ins))->isIntegerCast()) t = 'N';
							else t = 'F';
						}

						newNode.type = t;

						if ((((Instruction*)(&*ins))->getOpcode() == Instruction::Load) || 		//handle load instructions
								(((Instruction*)(&*ins))->getOpcode() == Instruction::Store)) {

							newNode.isLoad = true;
						}
						else newNode.isLoad = false;

						if(((Instruction*)(&*ins))->getOpcode() == Instruction::PHI) {		// PHI instruction can execute if any
															// of the operands are available.
							newNode.ifAny = true;
							newNode.latency = 0;
						}
						else
						{
							newNode.ifAny = false;		//instruction executes when both the operands are available
							newNode.latency = 1;		//every node except PHI have latency 1

						}

						newGraph.insert(pair <Value*, clust_node> (&*ins, newNode));
						sumWt = sumWt + wt;

					}//for each ins
				}//for each block

				graphs.insert (pair <unsigned int, clust_graph> (id, newGraph));
				return true;
			}//FormNodes


			void AddDataEdges (Loop* L, unsigned int id) {		//Insert data dependent edges from producer to consumer instructions
				printf("id = %u\n",id);		//print graph id
				map <unsigned int, clust_graph>::iterator loopGraph = graphs.find (id);
				assert (loopGraph != graphs.end());

				for (map<Value*, clust_node>::iterator nodeIter = loopGraph->second.begin(); nodeIter != loopGraph->second.end(); nodeIter ++) { //for each node
					if (nodeIter->second.nodeType == DATANODE) continue;
					Value* ins = nodeIter->second.ins;
					for (User::op_iterator opnd = ((Instruction*)ins)->op_begin(), oe = ((Instruction*)ins)->op_end(); opnd != oe; opnd ++) { //for each use
						Value* val = opnd->get();
						map<Value*, clust_node>::iterator target = loopGraph->second.find(val);		//check if use is in the graph

						if (target != loopGraph->second.end()) {	//use is in the graph and is being produced by some instruction

							edgeID ++;	//form edge
							clust_edge newEdge;
							newEdge.target = &(nodeIter->second);
							newEdge.depType = DATADEP;
							newEdge.wt = nodeIter->second.wt;
							newEdge.id = edgeID;
							newEdge.backEdge = false;
							target->second.edges.push_back(newEdge);

							clust_edge outEdge;
							outEdge.target = &(target->second);
							outEdge.depType = DATADEP;
							outEdge.wt = nodeIter->second.wt;
							outEdge.id = edgeID;
							outEdge.backEdge = false;
							nodeIter->second.outgoingEdges.push_back(outEdge);

							if (target->second.nodeType == DATANODE) target->second.wt = target->second.wt + nodeIter->second.wt; //if producer is a data node, update its weight
						}

						else {		//data coming from outside, add a data node to the graph

							clust_node newNode;
							newNode.ins = val;
							newNode.id = ++ nodeID ;
							newNode.entryNode = false;
							newNode.wt = nodeIter->second.wt;
							newNode.nodeType = DATANODE;
							newNode.depth = 0;

							loopGraph->second.insert(pair <Value*, clust_node> (val, newNode));	//insert
							target = loopGraph->second.find(val);
							assert (target != loopGraph->second.end());

							edgeID ++;	//add an edge
							clust_edge newEdge;
							newEdge.target = &(nodeIter->second);
							newEdge.depType = DATADEP;
							newEdge.wt = nodeIter->second.wt;
							newEdge.id = edgeID;
							newEdge.backEdge = false;
							target->second.edges.push_back(newEdge);

							clust_edge outEdge;
							outEdge.target = &(target->second);
							outEdge.depType = DATADEP;
							outEdge.wt = nodeIter->second.wt;
							outEdge.id = edgeID;
							outEdge.backEdge = false;
							nodeIter->second.outgoingEdges.push_back(outEdge);

						}//data coming from outside
					}//for each use
				}//for each ins
			}//AddDataEdges


			void AddCtrlEdges (Loop* L, unsigned int id) {		//Insert control dependent edges

				PostDominatorTree* PDT = &getAnalysis<PostDominatorTree>();
				map <unsigned int, clust_graph>::iterator loopGraph = graphs.find (id);		//get loop graph
				assert (loopGraph != graphs.end());

				// list to hold instructions that have been found to be control dependent on a 
				// branch instructions that are data dependent on such instructions and also control 
				// dependent on the same branch need only store the data dependence

				list <Value*> dependents;

				for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; bi ++) { 	//iterate over blocks for each block

					BasicBlock* bbl = *bi;
					dependents.clear();
					unsigned int nSucc = 0;		//check that BBL has multiple successors, otherwise nothing is control-dependent on it

					for (succ_iterator succ = succ_begin(bbl), se = succ_end(bbl); succ != se; ++succ) {
						nSucc ++;
					}//for each successor block

					if (nSucc <= 1) continue;
					assert(nSucc == 2);

					Value* tail = bbl->getTerminator();	//find terminal instruction of outer block
					assert (tail);
					map <Value*, clust_node>::iterator dstNode = loopGraph->second.find(&*tail);
					assert (dstNode != loopGraph->second.end());

					for (Loop::block_iterator biInner = L->block_begin(), beInner = L->block_end(); biInner != beInner; biInner ++) {	//check whether other blocks are control dependent on it

						if (*biInner == bbl) continue;		//check distinct blocks
						bool postDominates = false;
						succ_iterator dominated = succ_end(bbl);
						for (succ_iterator succ = succ_begin(bbl), se = succ_end(bbl); succ != se; ++succ) { 	//check if inner block post-dominates at least one of the successors of the outer block
							bool flag = (PDT->dominates(PDT->getNode(*biInner), PDT->getNode(*succ)));
							postDominates |= flag;
							if (flag) dominated = succ;

						}

						if ((postDominates) && (!PDT->dominates(PDT->getNode(*biInner), PDT->getNode(bbl)))) {	//check that inner block does not post dominate outer block
							for (BasicBlock::iterator ins = (*biInner)->begin(), ie = (*biInner)->end(); ins != ie; ins++) { 	//create a control dep edge from each ins in inner block to terminal ins of outer block
								list <Value*>::iterator depIter = dependents.begin();	//first check instruction is not data dependent on any instruction in the dependents list
								for (; depIter != dependents.end(); depIter ++) {

									map <Value*, clust_node>::iterator producer = loopGraph->second.find (*depIter); 	//find the node for the producer instr
									assert (producer != loopGraph->second.end());

									map <Value*, clust_node>::iterator consumer = loopGraph->second.find (&*ins);	//find the node for the consumer instr
									assert (producer != loopGraph->second.end());

									list <clust_edge>::iterator edgeIter = producer->second.edges.begin();	//check each edge of the producer to see if any of them target the consumer
									for (; edgeIter != producer->second.edges.end(); edgeIter ++)
									{
										if (edgeIter->target == &(consumer->second)) break;
									}
									if (edgeIter != producer->second.edges.end()) break;

								}//first check instruction is not data dependent on any instruction in the dependents list

								dependents.push_back(&*ins);	//add instruction to dependents list
								if (depIter != dependents.end()) continue;
								assert (dominated != succ_end(bbl));	//add control dependence edge, check which successor is in question

								clust_dep edgTyp = CTRLDEP_0;
								if (dominated == succ_begin(bbl)) { edgTyp = CTRLDEP_0; }
								else { edgTyp = CTRLDEP_1; }
								map <Value*, clust_node>::iterator srcNode = loopGraph->second.find(&*ins);
								assert (srcNode != loopGraph->second.end());

								edgeID ++;	//create a new edge

								clust_edge newEdge;
								newEdge.target = &(srcNode->second);
								newEdge.depType = edgTyp;
								newEdge.backEdge = false;
								newEdge.id = edgeID;

								dstNode->second.edges.push_back(newEdge);


								clust_edge outEdge;
								outEdge.target = &(dstNode->second);
								outEdge.depType = edgTyp;
								outEdge.wt = newEdge.wt;
								outEdge.id = edgeID;
								outEdge.backEdge = false;
								srcNode->second.outgoingEdges.push_back(outEdge);

							}//create a control dep edge from each ins in inner block to terminal ins of outer block
						}//control dependence exists
					}//check whether inner block is control dependent on outer block
				}//for each block
			}//AddCtrlEdges

			void PrintDotGraph(list<clust_node> *gepNodes, unsigned int id) {
				map <unsigned int, clust_graph>::iterator loopGraph = graphs.find(id);		//get loop graph
				assert (loopGraph != graphs.end());


				char fileName[256];	//create file name
				sprintf(fileName, "%u.loop_analysis_graph.dot", id);

				FILE* lf = fopen(fileName, "w");	//open file
				fprintf(lf, "digraph loop_analysis_graph {\n");


				for (map<Value*, clust_node>::iterator nodeIter = loopGraph->second.begin(); nodeIter != loopGraph->second.end(); nodeIter ++) {
					if(nodeIter->second.nodeType == INSTNODE) {
						Instruction *inst = (Instruction *)(nodeIter->first);
						int opcode = inst->getOpcode();
						const char *I = inst->getOpcodeName(opcode);

						if(!nodeIter->second.isLoad) {
							if (nodeIter->second.type == 'N') 	//compute node
								fprintf(lf, "%u [label=\"\t%u %s\", shape=oval]\n", nodeIter->second.id,nodeIter->second.id, I );
							else if (nodeIter->second.type == 'F')
								fprintf(lf, "%u [label=\"%u %s\", shape=doublecircle]\n", nodeIter->second.id, nodeIter->second.id, I);
							else fprintf(lf, "%u [label=\"%u %s\", shape=triplecircle]\n", nodeIter->second.id,nodeIter->second.id, I );
						} else {
							if (nodeIter->second.type == 'N') 	//memory node
								fprintf(lf, "%u [ label=\"%u %s\", shape=octagon]\n", nodeIter->second.id,nodeIter->second.id,I);
							else if (nodeIter->second.type == 'F')
								fprintf(lf, "%u [label=\"%u %s\", shape=doubleoctagon]\n", nodeIter->second.id, nodeIter->second.id,I);
							else fprintf(lf, "%u [label=\"%u %s\", shape=tripleoctagon]\n", nodeIter->second.id, nodeIter->second.id, I);
						}
					}
					else if (nodeIter->second.nodeType == DATANODE) {
						fprintf(lf, "%u [shape=box,color=blue,label=\"%u  \"]\n", nodeIter->second.id, nodeIter->second.id);
					}

					for (list<clust_edge>::iterator edgIter = nodeIter->second.edges.begin();
							edgIter != nodeIter->second.edges.end(); edgIter ++) {	//for each edge
						if (edgIter->backEdge) continue;
						if (edgIter->depType == DATADEP) {
							fprintf(lf, "%u -> %u [label=\"\"]\n", nodeIter->second.id, edgIter->target->id);}
						else if (edgIter->depType == CTRLDEP_0)
							fprintf(lf, "%u -> %u [style=dashed,color=red,label=\"\"]\n",  nodeIter->second.id,
									edgIter->target->id);
						else fprintf(lf, "%u -> %u [style=dashed,color=blue,label=\"\"]\n",  nodeIter->second.id,
								edgIter->target->id);
					}
				}


				//-- print the replaced GEP instructions as well
				if(gepNodes->size() != 0) {
					for(list<clust_node>::iterator listIter = gepNodes->begin(); listIter != gepNodes->end(); listIter++) {

						const char *nodeText;
						switch(listIter->gepNodeType) {
							case GEP_ADD1: nodeText = "GEP_ADD1\0"; break;
							case GEP_ADD2: nodeText = "GEP_ADD2\0"; break;
							case GEP_MULT: nodeText = "GEP_MULT\0"; break;
							case GEP_SIZE: nodeText = "GEP_SIZE\0"; break;
						}
						fprintf(lf, "%u [label=\"\t%u %s\", style=filled, fillcolor=lightgrey, shape=oval]\n", listIter->id, listIter->id, nodeText);

						for(list<clust_edge>::iterator edgIter = listIter->outgoingEdges.begin();
								edgIter != listIter->outgoingEdges.end(); edgIter++) {	//for each edge
							if (edgIter->backEdge) continue;
							if (edgIter->depType == DATADEP)
								fprintf(lf, "%u -> %u [label=\"\"]\n",  edgIter->gepTargetID, listIter->id);
							else if (edgIter->depType == CTRLDEP_0)
								fprintf(lf, "%u -> %u [style=dashed,color=red,label=\"\"]\n",  edgIter->gepTargetID, listIter->id);
							else fprintf(lf, "%u -> %u [style=dashed,color=blue,label=\"\"]\n",  edgIter->gepTargetID, listIter->id);
						}

						for(list<clust_edge>::iterator edgIter = listIter->edges.begin(); edgIter != listIter->edges.end(); edgIter++) { //for each edge
							if (edgIter->backEdge) continue;
							if (edgIter->depType == DATADEP)
								fprintf(lf, "%u -> %u [label=\"\"]\n", listIter->id, edgIter->gepTargetID);
							else if (edgIter->depType == CTRLDEP_0)
								fprintf(lf, "%u -> %u [style=dashed,color=red,label=\"\"]\n",  listIter->id, edgIter->gepTargetID);
							else fprintf(lf, "%u -> %u [style=dashed,color=blue,label=\"\"]\n",  listIter->id, edgIter->gepTargetID);
						}
					}
				}
				fprintf(lf, "}\n");
				fclose(lf);
			}


			void RemoveGEP(list<clust_node> *gepNodes, unsigned int id) {

				map <unsigned int, clust_graph>::iterator loopGraph = graphs.find(id);
				assert(loopGraph != graphs.end());

				for (map<Value*, clust_node>::iterator nodeIter = loopGraph->second.begin(); nodeIter != loopGraph->second.end(); nodeIter ++)
				{
					Value* ins = nodeIter->second.ins;

					if(isa<GEPOperator>(*&ins)) {

						clust_node newAddNode;
						newAddNode.ins = ins;
						newAddNode.isLoad = false;
						newAddNode.id = ++nodeID;
						newAddNode.entryNode = false;
						newAddNode.wt = wt;
						newAddNode.nodeType = INSTNODE;
						newAddNode.depth = 0;
						newAddNode.gepNodeType = GEP_ADD1;

						clust_node newAdd2Node;
						newAdd2Node.ins = nodeIter->second.ins;
						newAdd2Node.id = ++nodeID ;
						newAdd2Node.entryNode = false;
						newAdd2Node.wt = wt;
						newAdd2Node.nodeType = INSTNODE;
						newAdd2Node.depth = 0;
						newAdd2Node.gepNodeType = GEP_ADD2;

						clust_node newMultNode;
						newMultNode.ins = nodeIter->second.ins;
						newMultNode.id = ++nodeID ;
						newMultNode.entryNode = false;
						newMultNode.wt = wt;
						newMultNode.nodeType = INSTNODE;
						newMultNode.gepNodeType = GEP_MULT;
						newMultNode.depth = 0;


						// connect edge between add2 to add1
						clust_edge newEdge1;
						newEdge1.gepTargetID = newAdd2Node.id;
						newEdge1.depType = DATADEP;
						newEdge1.id = ++edgeID;
						newEdge1.backEdge = false;
						newAddNode.outgoingEdges.push_back(newEdge1);

						// connect mul to add2
						clust_edge newEdgeMulAdd2;
						newEdgeMulAdd2.gepTargetID = newMultNode.id;
						newEdgeMulAdd2.depType = DATADEP;
						newEdgeMulAdd2.id = ++edgeID;
						newEdgeMulAdd2.backEdge = false;
						newAdd2Node.outgoingEdges.push_back(newEdgeMulAdd2);

						clust_node newSizeNode;
						newSizeNode.ins = nodeIter->second.ins;
						newSizeNode.id = ++nodeID ;
						newSizeNode.entryNode = false;
						newSizeNode.wt = wt;
						newSizeNode.nodeType = INSTNODE;
						newSizeNode.depth = 0;
						newSizeNode.gepNodeType = GEP_SIZE;

						//-- connect size to mul
						clust_edge newEdgeSizeMul;
						newEdgeSizeMul.gepTargetID = newSizeNode.id;
						newEdgeSizeMul.depType = DATADEP;
						newEdgeSizeMul.id = ++edgeID;
						newEdgeSizeMul.backEdge = false;
						newMultNode.outgoingEdges.push_back(newEdgeSizeMul);

						//-- connect add1 to the original child node of the GEP
						for(list<clust_edge>::iterator edgIter = nodeIter->second.edges.begin();
								edgIter != nodeIter->second.edges.end(); edgIter++) {
							clust_edge newEdgeAddGEPEnd;
							newEdgeAddGEPEnd.gepTargetID = edgIter->target->id;
							newEdgeAddGEPEnd.depType = edgIter->depType;
							newEdgeAddGEPEnd.id = ++edgeID;
							newEdgeAddGEPEnd.backEdge = edgIter->backEdge;
							newAddNode.edges.push_back(newEdgeAddGEPEnd);

						}

						//-- connect the incoming edges of the original GEP
						GEPOperator *instr = dyn_cast<GEPOperator>(*&ins);
						for(list<clust_edge>::iterator edgIter = nodeIter->second.outgoingEdges.begin();
								edgIter != nodeIter->second.outgoingEdges.end(); edgIter++) {
							clust_edge newEdgeGEP;
							newEdgeGEP.gepTargetID = edgIter->target->id;
							newEdgeGEP.depType = edgIter->depType;
							newEdgeGEP.id = ++edgeID;
							newEdgeGEP.backEdge = edgIter->backEdge;


							if(edgIter->target->ins == instr->getOperand(0)) {
								newAddNode.outgoingEdges.push_back(newEdgeGEP);
							} else if(edgIter->target->ins == instr->getOperand(1)) {
								newAdd2Node.outgoingEdges.push_back(newEdgeGEP);
							} else {
								newMultNode.outgoingEdges.push_back(newEdgeGEP);

							}
						}


						gepNodes->push_back(newAddNode);
						gepNodes->push_back(newAdd2Node);
						gepNodes->push_back(newMultNode);
						gepNodes->push_back(newSizeNode);

						// deleting the incoming edges of GEP node
						for(list<clust_edge> :: iterator itr = nodeIter->second.outgoingEdges.begin();
								itr != nodeIter->second.outgoingEdges.end(); itr++) {
							for(list<clust_edge>:: iterator parIter = itr->target->edges.begin();
									parIter != itr->target->edges.end(); parIter++) {
								if(parIter->target->id == nodeIter->second.id)
									parIter = itr->target->edges.erase(parIter);
							}
						}

						//deleting the outgoing edges of GEP node
						for(list<clust_edge> :: iterator itr = nodeIter->second.edges.begin();
								itr != nodeIter->second.edges.end(); itr++) {
							for(list<clust_edge>:: iterator parIter = itr->target->outgoingEdges.begin();
									parIter != itr->target->outgoingEdges.end(); parIter++) {
								if(parIter->target->id == nodeIter->second.id)
									parIter = itr->target->outgoingEdges.erase(parIter);
							}
						}

						for(list<clust_edge>::iterator Iter=nodeIter->second.outgoingEdges.begin();
								Iter != nodeIter->second.outgoingEdges.end(); Iter++) {
							Iter = nodeIter->second.outgoingEdges.erase(Iter);
						}

						for(list<clust_edge>::iterator edgIter=nodeIter->second.edges.begin(); edgIter != nodeIter->second.edges.end(); edgIter++) {
							edgIter = nodeIter->second.edges.erase(edgIter);
						}

					} // if GEP node


					//	Deleting the GEP node

					if(isa<GEPOperator>(*&ins) && (nodeIter->second.nodeType == INSTNODE)) {
						map<Value*, clust_node>::iterator eraseItr = nodeIter;
						++nodeIter;
						loopGraph->second.erase(eraseItr);
					}
				} // end for node iterator
			} // end function



			void RemoveCycle (list<clust_node*>& nodeStack) {

				if (nodeStack.empty()) return;
				clust_node* topNode = nodeStack.back();

				for (list<clust_edge>::iterator edgIter = topNode->edges.begin(); 	//process children
						edgIter != topNode->edges.end(); edgIter ++) {

					if (edgIter->backEdge) continue; //ignore backedges

					bool backedge = false;		//if child is in stack already, we have detected a backedge

					for (list<clust_node*>::iterator stackIter = nodeStack.begin(); stackIter != nodeStack.end(); stackIter ++)	{
						if (*stackIter != edgIter->target) continue;
						backedge = true;
						break;
					}//if child is in stack already, we have detected a backedge

					if (backedge) {	//mark edge as backedge
						edgIter->backEdge = true;
						topNode->nBackEdgesOut ++;
						for (list<clust_edge>::iterator edgIter2 = edgIter->target->outgoingEdges.begin();
								edgIter2 != edgIter->target->outgoingEdges.end(); edgIter2 ++) {

							if (edgIter2->target != topNode) continue;
							edgIter2->backEdge = true;
							edgIter->target->nBackEdgesIn ++;
							break;
						}
					}//mark edge as backedge

					else //push child
					{
						nodeStack.push_back(edgIter->target);
						edgIter->target->visited = true;
						RemoveCycle(nodeStack);
					}//push child
				}//process children

				nodeStack.pop_back();
			}//RemoveCycle

			bool WriteLoopGraph(unsigned int id, double cov) {

				map <unsigned int, clust_graph>::iterator loopGraph = graphs.find (id); //get loop graph
				assert (loopGraph != graphs.end());

				char fileName[256];	//create file name
				sprintf(fileName, "%u.loop_analysis_graph.graph", id);

				unsigned int maxDepth = 0; //calculate depths, should not exceed no of nodes

				//we do not really need depth we need to remove the cycles

				list <clust_node*> nodeStack;

				for (map<Value*, clust_node>::iterator nodeIter = loopGraph->second.begin(); nodeIter != loopGraph->second.end(); nodeIter ++) { //initialize

					nodeIter->second.visited = false;
					nodeIter->second.nBackEdgesIn = 0;
					nodeIter->second.nBackEdgesOut = 0;
				}

				for (map<Value*, clust_node>::iterator nodeIter = loopGraph->second.begin(); nodeIter != loopGraph->second.end(); nodeIter ++) { //start DFS

					if (nodeIter->second.visited) continue;		//init stack
					nodeStack.push_back(&(nodeIter->second));
					nodeIter->second.visited = true;
					RemoveCycle(nodeStack);

				}//DFS

				FILE* lf = fopen(fileName, "w");	//open file

				fprintf(lf, "%lu\t%u\t%.5lf\n", loopGraph->second.size(), maxDepth, cov);	//print no of vertices

				//print each vertex in order of id visit nodes in order of id
				for (unsigned int idCtr = 1; idCtr <= loopGraph->second.size(); idCtr ++) {
					for (map<Value*, clust_node>::iterator nodeIter = loopGraph->second.begin(); nodeIter != loopGraph->second.end(); nodeIter ++) {

						if (nodeIter->second.id != idCtr) continue;

						if ((nodeIter->second.nodeType == INSTNODE) && (!nodeIter->second.isLoad)) {

							fprintf(lf, "%u\t%.0lf\tC\t%c", nodeIter->second.depth, nodeIter->second.wt, nodeIter->second.type);

							fprintf(lf, "\t%ld", ((long)nodeIter->second.edges.size()) - nodeIter->second.nBackEdgesOut);//print no of outgoing edges
						}//compute node
						else if (nodeIter->second.nodeType == INSTNODE) {
							fprintf(lf, "%u\t%.0lf\tM\t%c", nodeIter->second.depth, nodeIter->second.wt, nodeIter->second.type);
							fprintf(lf, "\t%ld", ((long)nodeIter->second.edges.size()) - nodeIter->second.nBackEdgesOut); //print no of outgoing edges
						}//load node
						else fprintf(lf, "%u\t%.0lf\tD\tN\t%ld", nodeIter->second.depth, nodeIter->second.wt,
								((long)nodeIter->second.edges.size()) - nodeIter->second.nBackEdgesOut);

						//for each edge
						for (list<clust_edge>::iterator edgIter = nodeIter->second.edges.begin(); edgIter != nodeIter->second.edges.end();
								edgIter ++)
						{
							if (edgIter->backEdge) continue;

							if (edgIter->depType == DATADEP)
								fprintf(lf, "\t%u\tD\t%.0lf", edgIter->target->id, edgIter->target->wt);
							else if (edgIter->depType == CTRLDEP_0)
								fprintf(lf, "\t%u\tY\t%.0lf", edgIter->target->id, edgIter->target->wt);
							else
								fprintf(lf, "\t%u\tN\t%.0lf", edgIter->target->id, edgIter->target->wt);
						}//for each edge

						fprintf(lf, "\t%ld", ((long)nodeIter->second.outgoingEdges.size()) - nodeIter->second.nBackEdgesIn);

						for (list<clust_edge>::iterator edgIter = nodeIter->second.outgoingEdges.begin(); 	//for each incoming edge
								edgIter != nodeIter->second.outgoingEdges.end(); edgIter ++)
						{
							if (edgIter->backEdge) continue;
							if (edgIter->depType == DATADEP)
								fprintf(lf, "\t%u\tD\t%.0lf", edgIter->target->id, edgIter->target->wt);
							else if (edgIter->depType == CTRLDEP_0)
								fprintf(lf, "\t%u\tY\t%.0lf", edgIter->target->id, nodeIter->second.wt);
							else fprintf(lf, "\t%u\tN\t%.0lf", edgIter->target->id, nodeIter->second.wt);
						}//for each incoming edge

						fprintf(lf, "\n");
						break;
					}//for each node
				}//for each id
				return 1;
			}//WriteLoopGraph
	};
}

char LoopGraphAnalysisPass_0::ID = 0;
static RegisterPass<LoopGraphAnalysisPass_0> Z("loop-graph-analysis-0", "form program graphs and analyze");
