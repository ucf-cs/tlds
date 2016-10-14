TxList set1;
TxSkiplist set2;
TxMDList set3;
		
TxDesc* desc = new TxDesc({
		new Set<int>::InsertOp(3, set1), 
		new Set<int>::DeleteOp(6, set2), 
		new Set<int>::FindOp(5, set1), 
		new Set<int>::InsertOp(7, set3)});
desc->Execute();