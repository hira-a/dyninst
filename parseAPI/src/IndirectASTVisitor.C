#include "dyntypes.h"
#include "IndirectASTVisitor.h"
#include "debug_parse.h"
#include <algorithm>

using namespace Dyninst::ParseAPI;

AST::Ptr SimplifyVisitor::visit(DataflowAPI::RoseAST *ast) {
        unsigned totalChildren = ast->numChildren();
	for (unsigned i = 0 ; i < totalChildren; ++i) {
	    ast->child(i)->accept(this);
	    ast->setChild(i, SimplifyRoot(ast->child(i), size));
	}
	return AST::Ptr();
}

AST::Ptr SimplifyRoot(AST::Ptr ast, uint64_t insnSize) {
    if (ast->getID() == AST::V_RoseAST) {
        RoseAST::Ptr roseAST = boost::static_pointer_cast<RoseAST>(ast); 
	
	switch (roseAST->val().op) {
	    case ROSEOperation::invertOp:
	        if (roseAST->child(0)->getID() == AST::V_RoseAST) {
		    RoseAST::Ptr child = boost::static_pointer_cast<RoseAST>(roseAST->child(0));
		    if (child->val().op == ROSEOperation::invertOp) return child->child(0);
		} else if (roseAST->child(0)->getID() == AST::V_ConstantAST) {
		    ConstantAST::Ptr child = boost::static_pointer_cast<ConstantAST>(roseAST->child(0));
		    size_t size = child->val().size;
		    uint64_t val = child->val().val;
		    if (size < 64) {
		        uint64_t mask = (1ULL << size) - 1;
		        val = (~val) & mask;
		    } else
		        val = ~val;
		    return ConstantAST::create(Constant(val, size));
		}
		break;
	    case ROSEOperation::extendMSBOp:
	    case ROSEOperation::extractOp:
	    case ROSEOperation::signExtendOp:
	    case ROSEOperation::concatOp:
	        return roseAST->child(0);

	    case ROSEOperation::addOp:
	        if (roseAST->child(0)->getID() == AST::V_ConstantAST && roseAST->child(1)->getID() == AST::V_ConstantAST) {
		    ConstantAST::Ptr child0 = boost::static_pointer_cast<ConstantAST>(roseAST->child(0));
		    ConstantAST::Ptr child1 = boost::static_pointer_cast<ConstantAST>(roseAST->child(1));
		    uint64_t val = child0->val().val + child1->val().val;
		    size_t size;
		    if (child0->val().size > child1->val().size)
		        size = child0->val().size;
		    else
		        size = child1->val().size;
		    return ConstantAST::create(Constant(val,size));
   	        }
		if (roseAST->child(0)->getID() == AST::V_ConstantAST) {
		    ConstantAST::Ptr child = boost::static_pointer_cast<ConstantAST>(roseAST->child(0));
		    if (child->val().val == 0) return roseAST->child(1);
		}
		if (roseAST->child(1)->getID() == AST::V_ConstantAST) {
		    ConstantAST::Ptr child = boost::static_pointer_cast<ConstantAST>(roseAST->child(1));
		    if (child->val().val == 0) return roseAST->child(0);
		}

		break;
	    case ROSEOperation::sMultOp:
	    case ROSEOperation::uMultOp:
	        if (roseAST->child(0)->getID() == AST::V_ConstantAST) {
		    ConstantAST::Ptr child0 = boost::static_pointer_cast<ConstantAST>(roseAST->child(0));
		    if (child0->val().val == 1) return roseAST->child(1);
		}

	        if (roseAST->child(1)->getID() == AST::V_ConstantAST) {
		    ConstantAST::Ptr child1 = boost::static_pointer_cast<ConstantAST>(roseAST->child(1));
		    if (child1->val().val == 1) return roseAST->child(0);
		}
	        break;

	    case ROSEOperation::xorOp:
	        if (roseAST->child(0)->getID() == AST::V_VariableAST && roseAST->child(1)->getID() == AST::V_VariableAST) {
		    VariableAST::Ptr child0 = boost::static_pointer_cast<VariableAST>(roseAST->child(0)); 
		    VariableAST::Ptr child1 = boost::static_pointer_cast<VariableAST>(roseAST->child(1)); 
		    if (child0->val() == child1->val()) {
		        return ConstantAST::create(Constant(0 , 32));
		    }
  	        }
		break;
/*	    case ROSEOperation::derefOp:
	        // Any 8-bit value is bounded in [0,255].
		// Need to keep the length of the dereference if it is 8-bit.
		// However, dereference longer than 8-bit should be regarded the same.
	        if (roseAST->val().size == 8)
		    return ast;
		else
		    return RoseAST::create(ROSEOperation(ROSEOperation::derefOp), ast->child(0));
		break;
*/		
	    default:
	        break;

	}
    } else if (ast->getID() == AST::V_VariableAST) {
        VariableAST::Ptr varAST = boost::static_pointer_cast<VariableAST>(ast);
	if (varAST->val().reg.absloc().isPC()) {
	    MachRegister pc = varAST->val().reg.absloc().reg();
	    return ConstantAST::create(Constant(varAST->val().addr + insnSize, getArchAddressWidth(pc.getArchitecture()) * 8));
	}
	// We do not care about the address of the a-loc
	// because we will keep tracking the changes of 
	// each a-loc. Also, this brings a benefit that
	// we can directly use ast->isStrictEqual() to 
	// compare two ast.
	return VariableAST::create(Variable(varAST->val().reg));
    }
    return ast;
}


AST::Ptr SimplifyAnAST(AST::Ptr ast, uint64_t size) {
    SimplifyVisitor sv(size);
    ast->accept(&sv);
    return SimplifyRoot(ast, size);
}

AST::Ptr BoundCalcVisitor::visit(DataflowAPI::RoseAST *ast) {
    BoundValue *astBound = boundFact.GetBound(ast);
    if (astBound != NULL) {
        bound.insert(make_pair(ast, new BoundValue(*astBound)));
        return AST::Ptr();
    }
    unsigned totalChildren = ast->numChildren();
    for (unsigned i = 0 ; i < totalChildren; ++i) {
        ast->child(i)->accept(this);
    }
    switch (ast->val().op) {
        case ROSEOperation::addOp:
	    if (IsResultBounded(ast->child(0)) && IsResultBounded(ast->child(1))) {	    
		BoundValue* val = new BoundValue(*GetResultBound(ast->child(0)));		
		val->Add(*GetResultBound(ast->child(1)));
		if (*val != BoundValue::top)
		    bound.insert(make_pair(ast, val));
	    }	    
	    break;
	case ROSEOperation::invertOp:
	    if (IsResultBounded(ast->child(0))) {
	        BoundValue *val = new BoundValue(*GetResultBound(ast->child(0)));
		if (val->tableReadSize)
		    val->isInverted = true;
		else {
		    val->Invert();
		}
		if (*val != BoundValue::top)
		    bound.insert(make_pair(ast,val));
	    }
	    break;
	case ROSEOperation::andOp:{
	    // For and operation, even one of them is a top,
	    // we may still produce bound result.
	    // For example, top And 0[3,3] => 1[0,3]
	    BoundValue *val = NULL;
	    if (IsResultBounded(ast->child(0)))
	        val = new BoundValue(*GetResultBound(ast->child(0)));
	    else
	        val = new BoundValue(BoundValue::top);
	    if (IsResultBounded(ast->child(1)))
	        val->And(GetResultBound(ast->child(1))->interval);
	    else
	        val->And(StridedInterval::top);
	    // the result of an AND operation should not be
	    // the table lookup. Set all other values to default
	    val->ClearTableCheck();
	    if (*val != BoundValue::top)
	        bound.insert(make_pair(ast, val));
	    break;
	}
	case ROSEOperation::sMultOp:
	case ROSEOperation::uMultOp:
	    if (IsResultBounded(ast->child(0)) && IsResultBounded(ast->child(1))) {
	        BoundValue *val = new BoundValue(*GetResultBound(ast->child(0)));
	        val->Mul(*GetResultBound(ast->child(1)));
	        if (*val != BoundValue::top)
	            bound.insert(make_pair(ast, val));
	    }
	    break;
	case ROSEOperation::shiftLOp:
	    if (IsResultBounded(ast->child(0)) && IsResultBounded(ast->child(1))) {
	        BoundValue *val = new BoundValue(*GetResultBound(ast->child(0)));
	        val->ShiftLeft(*GetResultBound(ast->child(1)));
	        if (*val != BoundValue::top)
	            bound.insert(make_pair(ast, val));
	    }
	    break;
	case ROSEOperation::shiftROp:
	    if (IsResultBounded(ast->child(0)) && IsResultBounded(ast->child(1))) {
	        BoundValue *val = new BoundValue(*GetResultBound(ast->child(0)));
	        val->ShiftRight(*GetResultBound(ast->child(1)));
	        if (*val != BoundValue::top)
	            bound.insert(make_pair(ast, val));
	    }
	    break;
	case ROSEOperation::derefOp: 
	    if (IsResultBounded(ast->child(0))) {
	        BoundValue *val = new BoundValue(*GetResultBound(ast->child(0)));
		val->MemoryRead(block, ast->val().size / 8);
	        if (*val != BoundValue::top)
	            bound.insert(make_pair(ast, val));
	    } else if (handleOneByteRead && ast->val().size == 8) {
	        // Any 8-bit value is bounded in [0,255]
		// But I should only do this when I know the read 
		// itself is not a jump table
	        bound.insert(make_pair(ast, new BoundValue(StridedInterval(1,0,255))));
	    }
	    break;
	case ROSEOperation::orOp: {
	    if (IsResultBounded(ast->child(0)) && IsResultBounded(ast->child(1))) {
	        BoundValue *val = new BoundValue(*GetResultBound(ast->child(0)));
	        val->Or(*GetResultBound(ast->child(1)));
	        if (*val != BoundValue::top)
	            bound.insert(make_pair(ast, val));
	    }
	    break;

        }
	default:
	    break;
    }
    return AST::Ptr();
   
}

AST::Ptr BoundCalcVisitor::visit(DataflowAPI::ConstantAST *ast) {
    const Constant &v = ast->val();
    int64_t value = v.val;
    if (v.size != 1 && v.size != 64 && (value & (1ULL << (v.size - 1)))) {
        // Compute the two complements in bits of v.size
	// and change it to a negative number
        value = -(((~value) & ((1ULL << v.size) - 1)) + 1);
    }
    bound.insert(make_pair(ast, new BoundValue(value)));
    return AST::Ptr();
}

AST::Ptr BoundCalcVisitor::visit(DataflowAPI::VariableAST *ast) {
    BoundValue *astBound = boundFact.GetBound(ast);
    if (astBound != NULL) 
        bound.insert(make_pair(ast, new BoundValue(*astBound)));
    return AST::Ptr();
}

BoundValue* BoundCalcVisitor::GetResultBound(AST::Ptr ast) {
    if (IsResultBounded(ast)) {
	return bound.find(ast.get())->second;
    } else {
	return NULL;
    }	    
}

BoundCalcVisitor::~BoundCalcVisitor() {
    for (auto bit = bound.begin(); bit != bound.end(); ++bit)
        if (bit->second != NULL)
	    delete bit->second;
    bound.clear();
}


AST::Ptr JumpCondVisitor::visit(DataflowAPI::RoseAST *ast) {
    if (ast->val().op == ROSEOperation::invertOp) {
        invertFlag = true;
	return AST::Ptr();
    }
    unsigned totalChildren = ast->numChildren();
    for (unsigned i = 0 ; i < totalChildren; ++i) {
        ast->child(i)->accept(this);
    }
    return AST::Ptr();
}

AST::Ptr ComparisonVisitor::visit(DataflowAPI::RoseAST *ast) {
    // For cmp type instruction setting zf
    // Looking like <eqZero?>(<add>(<V([x86_64::rbx])>,<Imm:8>,),)
    // Assuming ast has been simplified
    if (ast->val().op == ROSEOperation::equalToZeroOp) {
        bool minuendIsZero = true;
        AST::Ptr child = ast->child(0);	
	if (child->getID() == AST::V_RoseAST) {
	    RoseAST::Ptr childRose = boost::static_pointer_cast<RoseAST>(child);
	    if (childRose->val().op == ROSEOperation::addOp) {
	        minuendIsZero = false;
	        subtrahend = childRose->child(0);
		minuend = childRose->child(1);
		// If the minuend is a constant, then
		// the minuend is currently in its two-complement form
		if (minuend->getID() == AST::V_ConstantAST) {
		    ConstantAST::Ptr constAST = boost::static_pointer_cast<ConstantAST>(minuend);
		    uint64_t val = constAST->val().val;
		    int size = constAST->val().size;
		    if (size < 64) 
		        val = ((~val)+ 1) & ((1ULL << size) - 1);
		    else if (size == 64)
		        val = (~val) + 1;
		    else
		        parsing_printf("WARNING: constant bit size %d exceeds 64!\n", size);
		    minuend = ConstantAST::create(Constant(val, size));
		} else {
		    // Otherwise, the minuend ast is in the form of add(invert(minuend), 1)
		    // Need to extract the real minuend
		    minuend = minuend->child(0)->child(0);
		}
	    } 	
	} 
	if (minuendIsZero) {
            // The minuend is 0, thus the add operation is subsume.
             subtrahend = ast->child(0);
	     minuend = ConstantAST::create(Constant(0));
	}
    }
    return AST::Ptr();
}

AST::Ptr SubstituteAnAST(AST::Ptr ast, const BoundFact::AliasMap &aliasMap) {
    for (auto ait = aliasMap.begin(); ait != aliasMap.end(); ++ait)
        if (*ast == *(ait->first)) {
	    return ait->second;
	}
    unsigned totalChildren = ast->numChildren();
    for (unsigned i = 0 ; i < totalChildren; ++i) {
        ast->setChild(i, SubstituteAnAST(ast->child(i), aliasMap));
    }
    return ast;

}


AST::Ptr DeepCopyAnAST(AST::Ptr ast) {
    if (ast->getID() == AST::V_RoseAST) {
        RoseAST::Ptr roseAST = boost::static_pointer_cast<RoseAST>(ast);
	AST::Children kids;
        unsigned totalChildren = ast->numChildren();
	for (unsigned i = 0 ; i < totalChildren; ++i) {
	    kids.push_back(DeepCopyAnAST(ast->child(i)));
	}
	return RoseAST::create(ROSEOperation(roseAST->val()), kids);
    } else if (ast->getID() == AST::V_VariableAST) {
        VariableAST::Ptr varAST = boost::static_pointer_cast<VariableAST>(ast);
	return VariableAST::create(Variable(varAST->val()));
    } else if (ast->getID() == AST::V_ConstantAST) {
        ConstantAST::Ptr constAST = boost::static_pointer_cast<ConstantAST>(ast);
	return ConstantAST::create(Constant(constAST->val()));
    } else if (ast->getID() == AST::V_BottomAST) {
        BottomAST::Ptr bottomAST = boost::static_pointer_cast<BottomAST>(ast);
	return BottomAST::create(bottomAST->val());
    } else {
        fprintf(stderr, "ast type %d, %s\n", ast->getID(), ast->format().c_str());
        assert(0);	
    }
}
