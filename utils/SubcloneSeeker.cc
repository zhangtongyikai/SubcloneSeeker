/*
The MIT License (MIT)

Copyright (c) 2013 Yi Qiao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <assert.h>
#include <algorithm>
#include <numeric>
#include <cmath>

#include "EventCluster.h"
#include "Subclone.h"
#include "SegmentalMutation.h"

#define EPISLON (0.01)

sqlite3 *res_database;

static int _num_solutions;
static std::vector<int> _tree_depth;

using namespace SubcloneSeeker;

int treeDepth(TreeNode *root) {
	if(root->isLeaf())
		return 1;

	int max_subtree_depth = 0;
	for(size_t i=0; i<root->getVecChildren().size(); i++) {
		int subtree_depth = treeDepth(root->getVecChildren()[i]);
		if(subtree_depth > max_subtree_depth)
			max_subtree_depth = subtree_depth;
	}
	return 1+max_subtree_depth;
}

void TreeEnumeration(Subclone * root, std::vector<EventCluster> vecClusters, size_t symIdx);
void TreeAssessment(Subclone * root, std::vector<EventCluster> vecClusters);

int main(int argc, char* argv[])
{

	if(argc < 2) {
		std::cerr<<"Usage: "<<argv[0]<<" <cluster-archive-sqlite-db> [output-db]"<<std::endl;
		return(0);
	}

	res_database=NULL;

	sqlite3 *database;
	int rc;
	rc = sqlite3_open_v2(argv[1], &database, SQLITE_OPEN_READONLY, NULL);
	if(rc != SQLITE_OK) {
		std::cerr<<"Unable to open database "<<argv[1]<<std::endl;
		return(1);
	}

	// load mutation clusters
	EventCluster dummyCluster;
	std::vector<sqlite3_int64> clusterIDs = dummyCluster.vecAllObjectsID(database);

	if(clusterIDs.size() == 0) {
		std::cerr<<"Event cluster list is empty!"<<std::endl;
		return(1);
	}

	std::vector<EventCluster> vecClusters;
	for(size_t i=0; i<clusterIDs.size(); i++) {
		EventCluster newCluster;
		newCluster.unarchiveObjectFromDB(database, clusterIDs[i]);

		// load CNV events
		CNV dummyCNV;
		DBObjectID_vec memberCNV_IDs = dummyCNV.allObjectsOfCluster(database, newCluster.getId());
		for(size_t j=0; j<memberCNV_IDs.size(); j++) {
			CNV *newCNV = new CNV();
			newCNV->unarchiveObjectFromDB(database, memberCNV_IDs[j]);
			newCluster.addEvent(newCNV, false);
		}

		vecClusters.push_back(newCluster);
	}

	sqlite3_close(database);

	std::sort(vecClusters.begin(), vecClusters.end());
	std::reverse(vecClusters.begin(), vecClusters.end());

	// Mutation list read. Start to enumerate trees
	// 1. Create a node contains no mutation (symId = 0).
	// this node will act as the root of the trees
	Subclone *root = new Subclone();
	root->setFraction(-1);
	root->setTreeFraction(-1);

	if(argc >= 2) {
		int rc = sqlite3_open(argv[2], &res_database);
		if(rc != SQLITE_OK ) {
			std::cerr<<"Unable to open result database for writting."<<std::endl;
			return(1);
		}
	}
	
	TreeEnumeration(root, vecClusters, 0);	

	if(res_database != NULL) 
		sqlite3_close(res_database);

	if(_tree_depth.size()> 0)
		std::cout<<_num_solutions<<"\t"<<std::accumulate(_tree_depth.begin(), _tree_depth.end(), 0)/float(_tree_depth.size())<<std::endl;

	return 0;
}

// First of all, a tree traverser that will print out the tree.
// This will be used when the program decides to output a tree
	
// Tree Print Traverser. This one will just print the symbol id
// of the node that is given to it.
class TreePrintTraverser: public TreeTraverseDelegate {
	public:
		virtual void preprocessNode(TreeNode *node) {
			if(!node->isLeaf())
				std::cerr<<"(";
		}

		virtual void processNode(TreeNode * node) {
			std::cerr<<((Subclone *)node)->fraction()<<",";
		}

		virtual void postprocessNode(TreeNode *node) {
			if(!node->isLeaf())
				std::cerr<<")";
		}
};

// This function will recursively construct all possible tree structures
// using the given mutation list, starting with the mutation identified by symIdx
//
// The idea behind this is quite simple. If a node is not the last symbol on the 
// mutation list, it will try to add this particular node to every other existing
// node's children list. After one addition, the traverser calls the TreeEnumeration
// again but with a incremented symIdx, so that the next symbol can be treated in the
// same way. When the last symbol has been reached, the tree is printed out, in both
// Pre-Order and Post-Order, by the TreePrintTraverser class, so that each tree can be 
// uniquely identified.

void TreeEnumeration(Subclone * root, std::vector<EventCluster> vecClusters, size_t symIdx)
{	
	// Tree Enum Traverser. It will check if the last symbol has been
	// treated or not. If yes, the tree is complete and it will call
	// TreeAssessment to assess the viability of the tree; If no, it 
	// will add the current untreated symbol as a children to the node 
	// that is given to it, and call TreeEnumeration again to go into 
	// one more level of the recursion.
	class TreeEnumTraverser : public TreeTraverseDelegate {
	protected:
		// Some state variables that the TreeEnumeration
		// function needs to expose to the traverser
		std::vector<EventCluster>& _vecClusters;
		size_t _symIdx;
		Subclone *_floatNode;
		Subclone *_root;
		
	public:
		
		TreeEnumTraverser(std::vector<EventCluster>& vecClusters,
						  size_t symIdx,
						  Subclone *floatNode,
						  Subclone *root):
						_vecClusters(vecClusters), _symIdx(symIdx), 
						_floatNode(floatNode), _root(root) {;}
								
		
		virtual void processNode(TreeNode * node) {
			Subclone *clone = dynamic_cast<Subclone *>(node);
			
			// Add the floating node as the chilren of the current node
			clone->addChild(_floatNode);
			
			// Move on to the next symbol
			TreeEnumeration(_root, _vecClusters, _symIdx);
			
			// Remove the child
			node->removeChild(_floatNode);
		}
	};

	if(symIdx == vecClusters.size()) {
		TreeAssessment(root, vecClusters);
		return;
	}

	// Create the new tree node
	Subclone *newClone = new Subclone();
	newClone->setFraction(-1);
	newClone->setTreeFraction(-1);
	newClone->addEventCluster(&vecClusters[symIdx]);

	// add more cluster into the same subclone if they share
	// the same frequency. This is unlikely to happen if
	// the clusters are generated from a clustering algorithm
	// run on the raw data. But when using external dataset this
	// could be possible
	float currentFraction = vecClusters[symIdx].cellFraction();
	symIdx++;

	while(symIdx < vecClusters.size() && 
			fabs(vecClusters[symIdx].cellFraction() - currentFraction) < EPISLON) {
		newClone->addEventCluster(&vecClusters[symIdx]);
		symIdx++;
	}

	// Configure the tree traverser
	TreeEnumTraverser TreeEnumTraverserObj(vecClusters, symIdx, newClone, root);
	
	// Traverse the tree
	TreeNode::PreOrderTraverse(root, TreeEnumTraverserObj);
	
	delete newClone;
}

void TreeAssessment(Subclone * root, std::vector<EventCluster> vecClusters)
{
	class FracAsnTraverser : public TreeTraverseDelegate {
	protected:
		std::vector<EventCluster>& _vecClusters;
		
	public:
		FracAsnTraverser(std::vector<EventCluster>& vecClusters): _vecClusters(vecClusters) {;}
		virtual void processNode(TreeNode * node) {

			Subclone *clone = dynamic_cast<Subclone *>(node);

			if(node->isLeaf()) {
				// direct assign
				((Subclone *)node)->setFraction(((Subclone *)node)->vecEventCluster()[0]->cellFraction());
				((Subclone *)node)->setTreeFraction(((Subclone *)node)->vecEventCluster()[0]->cellFraction());
			}
			else {
				// intermediate node. assign it's mutation fraction - subtree_fraction
				// actually, if it's root, assign 1
				if(clone->isRoot()) 
					clone->setTreeFraction(1);
				else 
					clone->setTreeFraction(clone->vecEventCluster()[0]->cellFraction());
				
				assert(clone->treeFraction() >= -EPISLON && clone->treeFraction() <= 1 + EPISLON);
							
				double childrenFraction = 0;
				for(size_t i=0; i<node->getVecChildren().size(); i++) 
					childrenFraction += ((Subclone *)node->getVecChildren()[i])->treeFraction();
									
				double nodeFraction = ((Subclone *)node)->treeFraction() - childrenFraction;

				if(nodeFraction < EPISLON && nodeFraction > -EPISLON)
					nodeFraction = 0;
				
				// check tree viability
				if(nodeFraction < -EPISLON) {
					terminate();
				}
				else {
					((Subclone *)node)->setFraction(nodeFraction);
					assert(((Subclone *)node)->fraction() >= -EPISLON && ((Subclone *)node)->fraction() <= 1+EPISLON);
				}
			}
		}
	};
	
	// Fraction Reset Traverser. This will go through the nodes and
	// reset the fraction to uninitialized state so that the same nodes
	// can be used for another structure's evaluation
	class NodeResetTraverser : public TreeTraverseDelegate {
	public:
		virtual void processNode(TreeNode * node) {
			((Subclone *)node)->setFraction(-1);
			((Subclone *)node)->setTreeFraction(-1);
			((Subclone *)node)->setParentId(0);
			((Subclone *)node)->setId(0);

		}
	};
		
	// check if the root is sane
	if(root == NULL)
		return;

	// reset node and tree fractions
	NodeResetTraverser nrTraverser;
	TreeNode::PreOrderTraverse(root, nrTraverser);

	// calcuate tree fractions
	FracAsnTraverser fracTraverser(vecClusters);
	TreeNode::PostOrderTraverse(root, fracTraverser);
	
	// if the tree is viable, output it
	if(root->fraction() >= -EPISLON) {
		TreePrintTraverser printTraverser;
		std::cerr<<"Viable Tree! Pre-Orer: ";
		TreeNode::PreOrderTraverse(root, printTraverser);
		std::cerr<<std::endl;

		// save tree to database
		if(res_database != NULL) {
			SubcloneSaveTreeTraverser stt(res_database);
			TreeNode::PreOrderTraverse(root, stt);
		}

		_num_solutions++;
		_tree_depth.push_back(treeDepth(root));
	}
	else
	{
		TreePrintTraverser printTraverser;
		std::cerr<<"Unviable Tree! Pre-Orer: ";
		TreeNode::PreOrderTraverse(root, printTraverser);
		std::cerr<<std::endl;

	}
}
