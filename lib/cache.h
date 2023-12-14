#ifndef CACHE_H
#define CACHE_H

#include <iostream>
#include <unordered_map>
#include <list>
#include <utility>
#include <vector>
const int DEFAULT_SIZE = 100000; //Tamaño de la caché por defecto

using namespace std;

struct Cache{
    int size; //Tamaño de la caché
    list<pair<int, Packet>> cache_list; //Lista de elementos de la caché
    unordered_map<int, list<pair<int, Packet>>::iterator> cache_map; //Mapa de elementos de la caché
    Cache(int size = DEFAULT_SIZE)
    {
        this->size = size;
        cache_list.clear();
        cache_map.clear();
    }
    ~Cache()
    {
        cache_list.clear();
        cache_map.clear();
    }

    //Insertar un elemento en la caché
    void insert(int key, Packet value)
    {
        //Si la caché está llena, eliminar el primer elemento
        if (cache_list.size() == size)
        {
            int key_to_delete = cache_list.back().first;
            cache_list.pop_back();
            cache_map.erase(key_to_delete);
        }
        //Insertar el elemento en la caché
        cache_list.push_front(make_pair(key, value));
        cache_map[key] = cache_list.begin();
    }

    //Obtener un elemento de la caché
    Packet get(int key)
    {
        //Si el elemento no está en la caché, devolver NULL
        if (cache_map.find(key) == cache_map.end())
        {
            return {};
        }
            
        //Si el elemento está en la caché, devolver su valor
        auto it = cache_map[key];
        Packet value = it->second;
        cache_list.erase(it);
        cache_list.push_front(make_pair(key, value));
        cache_map[key] = cache_list.begin();
        return value;
    }
};

#endif //CACHE_H