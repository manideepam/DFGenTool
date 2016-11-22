/*
 * DFGenTool is a Data Flow Graph (DFG) Generation Tool, which converts loops
 * in a sequential program given in high level language like C/C++ into a DFG.
 * This is the header file for loop_graph_0.cpp.
 * For complete list of authors refer to AUTHORS.txt.
 * For more details about the license refer to LICENSE.txt.
 * ----------------------------------------------------------------------------
 *
 * Copyright (C) 2012 Apala Guha
 * Copyright (C) 2016 Manideepa Mukherjee
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

#ifndef _LOOP_GRAPH_ANALYSIS_H_
#define _LOOP_GRAPH_ANALYSIS_H_ 1

#include "llvm/Support/CFG.h"
#include <map>
#include <list>

using namespace llvm;
using namespace std;

typedef enum
{
	DATADEP,
	CTRLDEP_0,
	CTRLDEP_1
}clust_dep;

/*typedef enum
{
	ONE_OP,
	ALL_OP

}clust_schedTime;
*/
/*typedef enum
{
	LATENCY_0,
	LATENCY_1

}clust_nodeLat;
*/
typedef enum
{
	DATANODE,
	INSTNODE
}clust_nodeType;

typedef enum {
	GEP_MULT,
	GEP_ADD1,
	GEP_ADD2,
	GEP_SIZE
} gep_nodeType;

struct my_clust_node;

typedef struct
{
	struct my_clust_node* target;
	clust_dep depType;
	double wt; //weight of edges
	unsigned int id;
	bool backEdge;
	unsigned int gepTargetID;

}clust_edge;

typedef struct my_clust_node
{
	Value* ins;
	list<clust_edge> edges;
	list<clust_edge> outgoingEdges;
	unsigned int id;
	bool entryNode;
	bool ifAny;
	double wt; //weight of node, i.e. no of time operation executes
	clust_nodeType nodeType;
	bool  lvisited;
	list<clust_edge>::iterator nextEdgToVisit;
	unsigned int depth;
	int latency;
	char type; //int/float/vector
	bool isLoad;
	bool visited; //used in depth-first search
	int nBackEdgesIn;
	int nBackEdgesOut;
	gep_nodeType gepNodeType;
} clust_node;

typedef map<Value*, clust_node> clust_graph;

#endif //_LOOP_GRAPH_ANALYSIS_H_
