all:
	g++ -o main main.cpp -lpthread
	g++ -o storage storage.cpp -lpthread
	g++ -o client client.cpp -lpthread
	clear
	
clean:
	rm -f main storage client
	clear