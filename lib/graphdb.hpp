#ifndef GRAPHDB_HPP
#define GRAPHDB_HPP

#include <map>
#include <set>
#include <string>
#include <iostream>

#include <vector>
#include <utility>
using Edge = std::pair<std::string, std::string>;

std::vector<std::string> edgeListToString (std::vector<Edge> list, int packet_data_size);
std::vector<Edge> stringToEdgeList (std::string data);

class GraphDB {
    
    std::map<std::string, std::set<std::string> > adj;
    
public:
    //Create connection between from and to
    void
    addEdge(std::string const& from, std::string const& to)
    {
        adj[from].insert(to);
    }
    
    //Delete connection between from and to
    void
    deleteEdge(std::string const& from, std::string const& to)
    {
        adj[from].erase(to);
    }

    //Delete all connections node a currently has
    void
    deleteNode(std::string const& a)
    {
        adj.erase(a);
    }
    
    //Update connection of from to old_to to from to new_to
    void
    updateEdge(std::string const& from, std::string const& old_to, std::string const& new_to)
    {
        deleteEdge(from, old_to);
        addEdge(from, new_to);
    }
    
    std::vector<Edge>
    getEdges(std::string const& from, int recursive = 1)
    {
        std::vector<Edge> edges;
        edges.reserve(adj[from].size());
        for (auto const& to : adj[from])
            edges.emplace_back(from, to);

        return edges;
    }

    //Returns a vector of strings, each one containing the result of read request.
    //Only 1 string if it fits in one packet, otherwise split in multiple strings
    //for multiple packets
    std::vector<std::string>
    getEdgesAsString(std::string const& from, int packet_data_size, int recursive = 1)
    {
        std::vector<Edge> edges = getEdges(from, recursive);
        return edgeListToString(edges, packet_data_size);
    }

    //Number of nodes in the database
    int
    size()
    {
        return adj.size();
    }

    //Check if a is connected to b
    bool
    hasRelation(std::string a, std::string b)
    {
        return (adj[a].find(b) != adj[a].end());
    }

    //Check if node a exists in the database
    bool
    exists(std::string a)
    {
        return (adj.find(a) != adj.end());
    }

    //Print information about the database
    void
    printInfo()
    {
        int total = 0;
        std::cout << "#Nodes: " << adj.size() << std::endl;
        std::cout << "---------- Nodes and # of connections ----------" << std::endl;
        for (auto pair : adj)
        {
            total += pair.second.size();
            std::cout << pair.first << ": ";
            for (auto elem : pair.second)
            {
                std::cout << elem << ", ";
            }
            std::cout << std::endl;
        }
        std::cout << "# All connections: " << total << std::endl;
    } 
};

// Return a vector of data strings of type response
std::vector<std::string>
edgeListToString (std::vector<Edge> list, int packet_data_size)
{
    std::map<std::string, std::set<std::string>> buff;
    std::string out;
    std::vector<std::string> result;

    for (int i = 0; i < list.size(); i++)
    {
        buff[list[i].first].insert(list[i].second);
    }

    for (auto pair : buff)
    {
        out += pair.first + ":";
        for (auto elem : pair.second)
        {
            if ((out+elem).size()+1 > packet_data_size)
            {
                if (out[out.size()-1] == ':')
                {
                    out.erase(out.end()-2, out.end());
                }
                else
                {
                    out[out.size()-1] = ';';
                }
                result.push_back(out);
                out = pair.first + ":";
            }
            out += elem + ",";
        }
        out[out.size()-1] = ';';
    }
    result.push_back(out);

    return result;
}

std::vector<Edge>
stringToEdgeList (std::string data)
{
    std::vector<Edge> result;
    std::string word;
    std::string currentFirst;
    for (int i = 0; i < data.size(); i++)
    {
        if (data[i] == ':')
        {
            currentFirst = word;
            word.clear();
        }
        else if (data[i] == ',' || data[i] == ';')
        {
            result.push_back(std::make_pair(currentFirst, word));
            word.clear();
        }
        else
        {
            word += data[i];
        }
    }
    return result;
}

#endif