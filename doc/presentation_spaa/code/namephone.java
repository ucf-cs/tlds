void ChangePhone(string name, string phone, string oldPhone){
	contactMap.Update(name, phone);
	phoneMap.Delete(oldPhone);
	phoneMap.Insert(phone, name);
}