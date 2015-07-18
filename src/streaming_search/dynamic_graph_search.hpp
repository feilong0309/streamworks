#ifndef __DYNAMIC_GRAPH_SEARCH_HPP__
#define __DYNAMIC_GRAPH_SEARCH_HPP__
#include <stdlib.h>
#include <string>
#include "sj_tree_executor.hpp"
#include "query_parser.hpp"
#include <assert.h>
#include "property_map.h"
#include "path_index.h"
#include "query_planning.hpp"
#include <limits.h>
using namespace std;

/*
enum RECORD_FORMAT {
    NETFLOW,
    NCCDC,
    WIN_EVNT_LOGS,
    RDF
};
*/

std::map<string, string> gPropertyMap;

string GetLogPathPrefix(string query_path)
{
    size_t pos = query_path.find_last_of("/");

    string dirpath, query_type;
    if (pos == string::npos) { 
        dirpath = ".";
    }
    else {
        dirpath = query_path.substr(0, pos);
    }
    
    char timestamp[64];
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    // strftime(buf, 1024, "cost_runtime_%F_%T.log", timeinfo);
    strftime(timestamp, 1024, "%F", timeinfo);

    if (pos == string::npos) {
        size_t pos1 = query_path.find("_");
        query_type = query_path.substr(0, pos1);
    }
    else {
        size_t pos1 = query_path.find("_", pos);
        query_type = query_path.substr(pos+1, pos1-pos-1);
    }

    string query_algo = GetProperty(gPropertyMap, "query_algo");
    string win_size = GetProperty(gPropertyMap, "num_edges_to_process_M");
    string search_offset = GetProperty(gPropertyMap, "search_offset");

    char buf[1024];
    sprintf(buf, "%s/%s_%s_%s_win%sM_offset%sM", dirpath.c_str(), 
            timestamp, query_algo.c_str(), query_type.c_str(),
            win_size.c_str(), search_offset.c_str());
    return string(buf);
}

string GetRuntimeLogPath(string query_path)
{
    string prefix = GetLogPathPrefix(query_path);
    char buf[1024];
    sprintf(buf, "%s_cost_runtime.csv", prefix.c_str());
    return string(buf);
}
 
void GetQueryCost(string query_path, float& single_edge_cost,
        float& path_cost)
{
    string selectivity_path = GetProperty(gPropertyMap, "selectivity_path");
    ifstream ifs(selectivity_path.c_str(), ios_base::in);
    string line;
    string q_path;
    single_edge_cost = -1.0;
    path_cost = -1.0;
    float cost1, cost2;

    while (ifs.good()) {
        getline(ifs, line);
        stringstream strm(line);
        strm >> q_path >> cost1 >> cost2;
        if (q_path == query_path) {
            single_edge_cost = cost1;
            path_cost = cost2;
            break;
        }
    }
    ifs.close();
    return;
}

void StoreRelativeSelectivity(float relative_selectivity)
{
    string rel_selectivity_path = GetProperty(gPropertyMap, "rel_selectivity_path");
    cout << "Saving selectivity at: "<< rel_selectivity_path << endl;
    string query_plan = GetProperty(gPropertyMap, "query_plan");

    ofstream ofs(rel_selectivity_path.c_str(), ios_base::app);
    ofs << query_plan << " " << relative_selectivity << endl;
    ofs.close();    
    return;
}

float GetRelativeSelectivity()
{
    string query_plan = GetProperty(gPropertyMap, "query_plan");
    string rel_selectivity_path = GetProperty(gPropertyMap, "rel_selectivity_path");

    cout << "Loading selectivity from: " << rel_selectivity_path << endl;
    std::map<string, string> s_map;
    LoadPropertyMap(rel_selectivity_path.c_str(), s_map);

    string rel_selectivity_str = GetProperty(s_map, query_plan);
    char* dummy;
    float relative_selectivity = strtof(rel_selectivity_str.c_str(),
            &dummy);
    return relative_selectivity;
}

template <class StreamingGraph, class FlowProcessor, class GraphProcessor>
void ProcessGraph(string data_path, RECORD_FORMAT input_format,
        int num_header_lines, GraphProcessor& graph_processor)
{
    FlowProcessor* flow_processor = new FlowProcessor(data_path, input_format, num_header_lines);
    cout << "Loading raw data ..." << endl;
    flow_processor->Load();
    cout << "Initializing graph ..." << endl;
    StreamingGraph* streaming_graph = flow_processor->InitGraph();
    cout << "Loading graph ..." << endl;
    flow_processor->LoadGraph();   
    cout << "Processing graph ..." << endl;
    graph_processor(streaming_graph);
    cout << "Cleaning up ..." << endl;
    delete streaming_graph;
    delete flow_processor;
    return;
}

template <class StreamingGraph, class FlowProcessor>
void CollectSubgraphStatistics(FlowProcessor* flow_processor)
{
    string path_distribution_data = GetProperty(gPropertyMap,
            "path_distribution_data");
    string edge_distribution_data = GetProperty(gPropertyMap,
            "edge_distribution_data");

    bool collect_path_distribution = !Exists(path_distribution_data);
    bool collect_edge_distribution = !Exists(edge_distribution_data);

    if (!collect_path_distribution && !collect_edge_distribution) {
        cout << "Path and edge distribution data already exists" << endl;
        cout << "   " << path_distribution_data << endl;
        cout << "   " << edge_distribution_data << endl;
        cout << "Skipping statistics collection phase" << endl;
        return;
    }
    
    struct timeval t1, t2;
    cout << "INFO CollectSubgraphStatistics: Initializing graph ..." << endl;
    StreamingGraph* gSearch = flow_processor->InitGraph();
    flow_processor->LoadGraph();   

    if (collect_path_distribution) {
        gettimeofday(&t1, NULL);
        float* path_distribution;
        int num_paths;
        StreamingGraph** paths = IndexPaths(gSearch, &path_distribution, num_paths,
                path_distribution_data);
        gettimeofday(&t2, NULL);
        delete[] path_distribution;
        for (int i = 0; i < num_paths; i++) {
            delete paths[i];
        }
        delete[] paths;
        cout << "IndexPaths : " << get_tv_diff(t1, t2) <<  " usecs" << endl;
    }

    if (collect_edge_distribution) {
        gettimeofday(&t1, NULL);
        std::map<int, float> edge_dist = GetEdgeDistribution(gSearch);
        gettimeofday(&t2, NULL);
        cout << "Computed edge distribution : " << get_tv_diff(t1, t2) <<  " usecs" << endl;
        string edge_distribution_data = GetProperty(gPropertyMap,
                "edge_distribution_data");
        StoreEdgeDistribution(edge_dist, edge_distribution_data);
    }
    
    delete gSearch;
    return;
}

template <class StreamingGraph, class FlowProcessor>
void CollectSubgraphStatistics(string data_path, RECORD_FORMAT input_format,
        int num_header_lines)
{
    string path_distribution_data = GetProperty(gPropertyMap, 
            "path_distribution_data");
    string edge_distribution_data = GetProperty(gPropertyMap, 
            "edge_distribution_data");

    bool collect_path_distribution = !Exists(path_distribution_data);
    bool collect_edge_distribution = !Exists(edge_distribution_data);

    if (!collect_path_distribution && !collect_edge_distribution) {
        return;
    }
    
    struct timeval t1, t2;
    FlowProcessor flow_processor(data_path, input_format, num_header_lines);
    cout << "Loading raw data ..." << endl;
    flow_processor.Load();
    cout << "Initializing graph ..." << endl;
    StreamingGraph* gSearch = flow_processor.InitGraph();
    // uint64_t num_edges_for_estimation = 1000000;
    flow_processor.LoadGraph();   

    if (collect_path_distribution) {
        gettimeofday(&t1, NULL);
        float* path_distribution;
        int num_paths;
        StreamingGraph** paths = IndexPaths(gSearch, &path_distribution, num_paths,
                path_distribution_data);
        gettimeofday(&t2, NULL);
        cout << "IndexPaths : " << get_tv_diff(t1, t2) <<  " usecs" << endl;
    }

    if (collect_edge_distribution) {
        gettimeofday(&t1, NULL);
        std::map<int, float> edge_dist = GetEdgeDistribution(gSearch);
        gettimeofday(&t2, NULL);
        cout << "Computed edge distribution : " << get_tv_diff(t1, t2) <<  " usecs" << endl;
        string edge_distribution_data = GetProperty(gPropertyMap,
                "edge_distribution_data");
        StoreEdgeDistribution(edge_dist, edge_distribution_data);
    }
    
    delete gSearch;
    return;
}

template <class StreamingGraph>
float GenerateQueryPlan(StreamingGraph* gQuery)
{
    // Load Statistics
    string path_distribution_data = GetProperty(gPropertyMap,
            "path_distribution_data");
    int num_paths;
    float* path_distribution;
    StreamingGraph** paths = LoadPathDistribution<StreamingGraph>(path_distribution_data,
            &path_distribution, num_paths);

    string edge_distribution_data = GetProperty(gPropertyMap,
            "edge_distribution_data");
    std::map<int, float> edge_distribution;
    LoadEdgeDistribution(edge_distribution_data, edge_distribution);
    
    // Run Decomposition
    string query_plan = GetProperty(gPropertyMap, "query_plan");
    bool decompose_paths = 
            (bool) atoi(GetProperty(gPropertyMap, "decompose_paths").c_str());
    if (decompose_paths) {
        cout << "INFO Performing path based decomposition." << endl; 
    }
    else {
        cout << "INFO Performing edge based decomposition." << endl; 
    }

    float sj_tree_cost;
    JoinTreeDAO* join_tree = DecomposeQuery(gQuery, edge_distribution,
            paths, path_distribution, num_paths, &sj_tree_cost, decompose_paths);

    cout << "Storing join tree at : " << query_plan << endl;
    if (join_tree->GetLeafCount() == gQuery->NumEdges()) {
        cout << "WARNING !!!" << endl;
        cout << "Query decomposed into 1-edge subgraphs" << endl;
    }
    join_tree->Store(query_plan.c_str());
    cout << "Query plan saved. " << endl;
    // Cleanup
    for (int i = 0; i < num_paths; i++) {
        // cout << "   ... deleting path[" << i << "]" << endl;
        delete paths[i];
    }
    // cout << "   ... delete[] paths" << endl;
    delete[] paths;
    // cout << "   ... delete[] path_distribution" << endl;
    delete[] path_distribution;
    // cout << "   ... delete join_tree" << endl;
    delete join_tree;
    cout << "Completed memory cleanup ..." << endl;

    if (decompose_paths) {
        // Compute Relative Selectivity
        cout << "Computing single edge distribution cost ...." << endl;
        float single_edge_decomposition_cost = 
                GetSingleEdgeDecompositionCost(gQuery, edge_distribution);
        float relative_selectivity = sj_tree_cost/single_edge_decomposition_cost;

        string selectivity_path = GetProperty(gPropertyMap, "selectivity_path");
        string query_path = GetProperty(gPropertyMap, "query_path");
        ofstream logger(selectivity_path.c_str(), ios_base::app);
        logger << query_path << " " << single_edge_decomposition_cost 
               << " " << sj_tree_cost << " " << relative_selectivity << endl;
        logger.close();
        return relative_selectivity;
    }
    else {
        return sj_tree_cost;
    }
}

template <class StreamingGraph, class FlowProcessor>
uint64_t RunSearch(FlowProcessor* flow_processor, 
        StreamingGraph* gQuery, const SearchContext& context,
        string query_plan)
{
    cout << "++++++ Running search ...." << endl; 
    StreamingGraph* gSearch = flow_processor->InitGraph();

    SJTreeExecutor<StreamingGraph> graph_search(gSearch, gQuery, 
            context, query_plan);
    string log_path_prefix = GetLogPathPrefix(query_plan);
    
    cout << "Starting incremental search ..." << endl;
    cout << "LOG PREFIX: " << log_path_prefix << endl;
    uint64_t run_time = flow_processor->Run(graph_search, 
            log_path_prefix);
    cout << "Cleaning up temporary data graph ..." << endl;
    delete gSearch;
    cout << "Done." << endl;
    return run_time;
}

template <class StreamingGraph, class FlowProcessor>
uint64_t RunSearch(StreamingGraph* gQuery, const SearchContext& context, 
        string data_path, 
        RECORD_FORMAT input_format,
        int num_header_lines, string query_plan, string property_path)
{
    FlowProcessor flow_processor(data_path, input_format, num_header_lines);
    flow_processor.SetPropertyPath(property_path);
    flow_processor.Load();
    StreamingGraph* gSearch = flow_processor.InitGraph();

    SJTreeExecutor<StreamingGraph> graph_search(gSearch, gQuery, 
            context, query_plan);
    string log_path_prefix = GetLogPathPrefix(query_plan);
    
    cout << "Starting incremental search ..." << endl;
    uint64_t run_time = flow_processor.Run(graph_search,
            log_path_prefix);
    return run_time;
}

template <class StreamingGraph, class FlowProcessor>
FlowProcessor* InitFlowProcessor(string property_path)
{
    LoadPropertyMap(property_path.c_str(), gPropertyMap);
    string data_path = gPropertyMap["data_path"];
    RECORD_FORMAT input_format = 
            (RECORD_FORMAT) atoi(GetProperty(gPropertyMap, "input_format").c_str());
    int num_header_lines = atoi(GetProperty(gPropertyMap, "num_header_lines").c_str());; 
    FlowProcessor* flow_processor = 
            new FlowProcessor(data_path, input_format, num_header_lines);
    flow_processor->SetPropertyPath(property_path);
    flow_processor->Load();
    return flow_processor;
}

template <class StreamingGraph, class FlowProcessor>
int FlowSearchRunner(FlowProcessor* flow_processor, 
        string property_path, string query_path)
{
    string query_plan;
    bool decompose_paths = 
            (bool) atoi(GetProperty(gPropertyMap, "decompose_paths").c_str());

    string suffix = decompose_paths ? ".2" : ".1";
    query_plan = query_path + ".plan" + suffix;
    gPropertyMap["query_plan"] = query_plan;
    gPropertyMap["query_path"] = query_path;

    string vertex_property_path = gPropertyMap["vertex_property_path"];
    string edge_property_path = gPropertyMap["edge_property_path"];
    RECORD_FORMAT input_format = 
            (RECORD_FORMAT) atoi(GetProperty(gPropertyMap, "input_format").c_str());
    int num_header_lines = atoi(GetProperty(gPropertyMap, "num_header_lines").c_str());; 

    cout << "Query : " << query_path << endl;
    cout << "Vertex properties : " << vertex_property_path << endl;
    cout << "Edge properties : " << edge_property_path << endl;

    // QueryParser<Label, NetflowAttbs> query_loader;
    typedef typename StreamingGraph::VertexDataType vertex_data_t;
    typedef typename StreamingGraph::EdgeDataType edge_data_t;
    QueryParser<vertex_data_t, edge_data_t> query_loader;
    StreamingGraph* gQuery = query_loader.ParseQuery(query_path.c_str(),
            vertex_property_path.c_str(),
            edge_property_path.c_str());
    SearchContext context = query_loader.search_context;
    assert(gQuery->NumEdges() != 0);

    CollectSubgraphStatistics<StreamingGraph, FlowProcessor>(flow_processor);

    float relative_selectivity = 0;
    if (!Exists(query_plan)) {
        cout << "Generating query plan ..." << endl;
        relative_selectivity = GenerateQueryPlan(gQuery);
        if (decompose_paths) {
            StoreRelativeSelectivity(relative_selectivity);
        }
    }
    else {
        relative_selectivity = 1;
        /* if (decompose_paths) {
            relative_selectivity = GetRelativeSelectivity();
        }
        else {
            relative_selectivity = 1;
        } */
        cout << "Relative selectivity: " << relative_selectivity << endl;
    }

    string run_search = GetProperty(gPropertyMap, "run_search");
    if (run_search == "yes") {
        uint64_t run_time = RunSearch(flow_processor, gQuery, context,
                query_plan);
    
        string logpath = GetRuntimeLogPath(query_path);
        ofstream logger(logpath.c_str(), ios_base::app);
        cout << "STATISTICS_COLLECTOR Storing runtime at: " << logpath << endl;
        float single_edge_cost, path_cost;
        GetQueryCost(query_path, single_edge_cost, path_cost);
        logger << query_path << "," << single_edge_cost << "," 
               << path_cost << "," << run_time << endl;
        logger.close();
    }
    delete gQuery;
    return 0;
}

template <class StreamingGraph, class FlowProcessor>
int FlowSearchRunner(string property_path, string query_path)
{
    LoadPropertyMap(property_path.c_str(), gPropertyMap);
    string data_path = gPropertyMap["data_path"];
    gPropertyMap["query_path"] = query_path;
    string query_plan;

    bool decompose_paths = 
            (bool) atoi(GetProperty(gPropertyMap, "decompose_paths").c_str());
    string suffix = decompose_paths ? ".2" : ".1";
    query_plan = query_path + ".plan" + suffix;
    gPropertyMap["query_plan"] = query_plan;

    string vertex_property_path = gPropertyMap["vertex_property_path"];
    string edge_property_path = gPropertyMap["edge_property_path"];
    RECORD_FORMAT input_format = 
            (RECORD_FORMAT) atoi(GetProperty(gPropertyMap, "input_format").c_str());
    int num_header_lines = atoi(GetProperty(gPropertyMap, "num_header_lines").c_str());; 

    cout << "Query : " << query_path << endl;
    cout << "Vertex properties : " << vertex_property_path << endl;
    cout << "Edge properties : " << edge_property_path << endl;

    // QueryParser<Label, NetflowAttbs> query_loader;
    typedef typename StreamingGraph::VertexDataType vertex_data_t;
    typedef typename StreamingGraph::EdgeDataType edge_data_t;
    QueryParser<vertex_data_t, edge_data_t> query_loader;
    StreamingGraph* gQuery = query_loader.ParseQuery(query_path.c_str(),
            vertex_property_path.c_str(),
            edge_property_path.c_str());
    SearchContext context = query_loader.search_context;
    assert(gQuery->NumEdges() != 0);

    CollectSubgraphStatistics<StreamingGraph, FlowProcessor>(data_path, 
            input_format, num_header_lines); 

    float relative_selectivity = 0;
    if (!Exists(query_plan)) {
        cout << "Generating query plan ..." << endl;
        relative_selectivity = GenerateQueryPlan(gQuery);
        bool decompose_paths = 
                (bool) atoi(GetProperty(gPropertyMap, "decompose_paths").c_str());
        if (decompose_paths) {
            StoreRelativeSelectivity(relative_selectivity);
        }
    }
    else {
        // relative_selectivity = GetRelativeSelectivity();
        cout << "Relative selectivity: " << relative_selectivity << endl;
    }

    string run_search = GetProperty(gPropertyMap, "run_search");
    if (run_search == "yes") {
        uint64_t run_time = RunSearch<StreamingGraph, FlowProcessor>(gQuery, 
                context, data_path, input_format, 
                num_header_lines, query_plan, property_path);
     
        string logpath = GetRuntimeLogPath(query_path);
        ofstream logger(logpath.c_str(), ios_base::app);
        cout << "STATISTICS_COLLECTOR Storing runtime at: " << logpath << endl;
        float single_edge_cost, path_cost;
        GetQueryCost(query_path, single_edge_cost, path_cost);
        logger << query_path << "," << single_edge_cost << "," 
               << path_cost << "," << run_time << endl;
        logger.close();
    }
    delete gQuery;
    return 0;
}

template <class StreamingGraph, class FlowProcessor>
int dynamic_graph_search_main(int argc, char* argv[])

{
   if(argc != 3) {
     cout << "Usage : <netflow.properties> <path_query_graph>";
     return 1;
   }
    if (argv[2] != NULL && strstr(argv[2], ".list")) {
        string property_path = argv[1];
        cout << "Parsing file list: " << argv[2] << endl;
        ifstream ifs(argv[2], ios_base::in);
        FlowProcessor* flow_processor = 
                InitFlowProcessor<StreamingGraph, FlowProcessor>(property_path);
        string line;
        vector<string> file_list;
        while (ifs.good()) {
            getline(ifs, line);
            if (line.size() && line[0] != '#') {
                file_list.push_back(line);
            }
        }
        ifs.close();
        for (int i = 0, N = file_list.size(); i < N; i++) {
            cout << "+++++++++++++++++++++++++++++++++++" << endl;
            cout << "Processing query: " << file_list[i] << endl;
            cout << "+++++++++++++++++++++++++++++++++++" << endl;
            FlowSearchRunner<StreamingGraph, FlowProcessor>(flow_processor, 
                    property_path, 
                    file_list[i]);
        }
        cout << "Cleaning up flow processor ..." << endl;
        delete flow_processor;
    }
    else {
        FlowSearchRunner<StreamingGraph, FlowProcessor>(argv[1], argv[2]);
    }
    return 0;
}
#endif
