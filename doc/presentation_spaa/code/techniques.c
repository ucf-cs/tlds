Desc* new_desc = new Desc();
Node* n = LocateNode();
do{
    Desc* curr_desc = n->desc;
    if(!IsTaskDone(curr_desc)) {
        HelpTask(curr_desc);
        continue;
    }
}while(!CAS(&n->desc, curr_desc, new_desc))
