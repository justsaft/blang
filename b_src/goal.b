
main(){
	extrn putchar;
	a = 4;
	b = 5;
	c = a + b;
	d = a - b;
	e = a * b;
	f = a / b;
	putchar(10);
	putchar(a);
	putchar(b);
	putchar(c);
	putchar(d);
	putchar(e);
	putchar(f);
	func();
}

func(){
	putchar(69);
}