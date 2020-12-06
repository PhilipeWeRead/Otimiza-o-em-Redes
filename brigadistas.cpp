///
/// UNIVERSIDADE FEDERAL DE OURO PRETO
/// PCC173 - OTIMIZAÇÃO EM REDES
/// 
/// Junho / 2019
/// 
/// Felipe César Lopes Machado - 16.2.5890
/// Philipe Lemos PArreira - 16.2.4517
/// Henrique Dutra Alvares - 13.2.4365
///
/// - PROBLEMA DOS BRIGADISTAS -
/// 5 brigadistas por turno.
/// 1 (um) foco inicial.

//#include "pch.h"
#include <iostream>
#include <chrono>
#include <fstream>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <set>
#include <queue>
#include <iomanip>
#include <algorithm>
#include <unordered_set>
#include <sstream>
#include <string>
#include <climits>

using namespace std;

#define IMPRIME_SEQUENCIA

#ifdef _WIN32 // Windows
	#define CLEAR "CLS"
#else
	#define CLEAR "clear"
#endif

#define PROGRESS(text) system(CLEAR); cout << text << flush
#define BRIGADISTAS_POR_TURNO 5

typedef enum { Queimado, Defendido, Normal } Estado;
typedef vector<vector<int>> Lista_Adj;
typedef vector<int> Distancia;
typedef vector<Estado> State;

int nvertices, narestas, nfocos, inicio, turn_count = 0, overlaps = 0, explored = 0;
int recursao = 1, last_vertex = -1;
Lista_Adj grafo;
vector<int> sequencia_final;
vector<int> vertex_local_tabu;
set<int> areas_finais;
unordered_set<string> tabu;

string instancia = "9.txt";

void get_input(int, char**);
void resolve();
void reset_states(State&);
bool next_area(set<int>&, const Lista_Adj&, State&);
int pode_proteger(const Lista_Adj&, set<int>&, State&, Distancia&, vector<int>&, bool&, int = 0, bool = true);
void rotula(const Lista_Adj&, Distancia&, State&);
int testa_sequencia();
void defender_sequencialmente(int brigadistas, State &state);
bool proximo_turno(Lista_Adj&, State&);
bool ja_foi_usado(set<int>&);
const string area_to_string(set<int>&);

template <typename T>
const string to_string_with_precision(const T a_value, const int precision = 1)
{
	ostringstream out;
	out.precision(precision);
	out << fixed << a_value;
	return out.str();
}

int main(int argc, char** argv) {

	ios_base::sync_with_stdio(false);

	get_input(argc, argv);

{
	using namespace chrono;
	cout << fixed << setprecision(3);
	high_resolution_clock::time_point tempo_inicio, tempo_fim;
	duration<double> time_span;

	tempo_inicio = high_resolution_clock::now(); // Início.

	resolve();
	const int res_final = testa_sequencia();

	tempo_fim = high_resolution_clock::now(); // Fim.
	time_span = duration_cast<duration<double>>(tempo_fim - tempo_inicio);

	// FIM DO ALGORITMO

#ifdef IMPRIME_SEQUENCIA
	cout << "Sequencia final: ";
	for (int a : sequencia_final)
		cout << a << " ";
	cout << "\n";
#endif

	cout << "Rodadas: " << turn_count << "\n";

	cout << "Salvos: --- " << res_final << " --- de " << nvertices << "\n"
		<< "Tempo: " << time_span.count() << " s\n";
}

	return EXIT_SUCCESS;
}

void get_input(int argc, char** argv)
{
	ifstream input(instancia);

	if (argc == 2) {
		input.close();
		input.open(argv[1]);
		instancia = argv[1];
	}

	if (!input.is_open()) {
		cerr << "Arquivo nao encontrado.\n";
		exit(2);
	}

	input >> nvertices >> narestas >> nfocos;
	assert(nfocos == 1);
	grafo.resize(nvertices);

	input >> inicio;

	int a, b;
	while (input >> a >> b) {
		assert(a < nvertices && b < nvertices);
		grafo[a].push_back(b);
		grafo[b].push_back(a);
	}

	input.close();
}

void resolve()
{
	set<int> maior_area_local, area;
	State estado;
	Distancia distancia;
	vector<int> melhor_ordem, ordem;
	const Lista_Adj graph(grafo);

	tabu.clear();

	int maior_qtde = 0, podados = 0, total_areas = 0;
	for (int i = 0; i < nvertices; ++i) {
		PROGRESS( "Instancia: " + instancia + "\n\n      === " + to_string(recursao) + " recursion ===" + "\nProgress... " + to_string_with_precision((float)(i + 1) / nvertices * 100) + " % (" + to_string(i + 1) + " of " + to_string(nvertices) + ")"
			+ "\nAreas explored: " + to_string(explored) + "\nOverlaps: " + to_string(overlaps) + "\nPruned: " + to_string_with_precision((float)podados / total_areas * 100) + " % (" + to_string(podados) + " total)\n\n");

		if (i == inicio || find(graph[inicio].begin(), graph[inicio].end(), i) != graph[inicio].end()
				|| areas_finais.find(i) != areas_finais.end() || find(sequencia_final.begin(), sequencia_final.end(), i) != sequencia_final.end()) // -inicial, adjacentes ao inicial e já seguros.
			continue;

		area.clear();
		area.insert(i);
		vertex_local_tabu.clear();

		do {
			reset_states(estado);
			ordem.clear();
			total_areas++;

			int rodada = sequencia_final.size() / BRIGADISTAS_POR_TURNO;
			bool prune = false;
			int res = pode_proteger(graph, area, estado, distancia, ordem, prune, rodada); // Altera estados.
			if (prune) {
				podados++;
				area.erase(last_vertex);
				vertex_local_tabu.push_back(last_vertex);
				continue;
			}
			else
				last_vertex = -1;

			if (res > maior_qtde) {
				maior_qtde = res;
				melhor_ordem = ordem;
				maior_area_local = area;
			}
		} while (next_area(area, graph, estado));
	}

	if (melhor_ordem.empty())
		return;

	sequencia_final.insert(sequencia_final.end(), melhor_ordem.begin(), melhor_ordem.end());
	areas_finais.insert(maior_area_local.begin(), maior_area_local.end());
	recursao++;

	resolve();
}

void reset_states(State& estado)
{
	estado.assign(nvertices, Normal);
	estado[inicio] = Queimado;
	for (int elem : sequencia_final) // Muda em cada recursão.
		estado[elem] = Defendido;
}

bool next_area(set<int>& conj, const Lista_Adj& graph, State& estado) {
	reset_states(estado); // Pois "pode_proteger()" altera estados.

	set<int> temp_set = conj;
	for (int vert : conj) {
		for (int adj : graph[vert]) {
			if (adj != inicio && find(vertex_local_tabu.begin(), vertex_local_tabu.end(), adj) == vertex_local_tabu.end() && find(graph[inicio].begin(), graph[inicio].end(), adj) == graph[inicio].end() 
					&& temp_set.find(adj) == temp_set.end() && estado[adj] == Normal) { // -inicial e adjacentes ao inicial, já pertencentes à área e defendidos.
				temp_set.insert(adj);
				if (!ja_foi_usado(temp_set)) { // Um por um.
					conj = temp_set;
					last_vertex = adj;
					explored++;
					return true;
				}
				overlaps++;
				temp_set.erase(adj);
			}
		}
	}
	return false;
}

int pode_proteger(const Lista_Adj& graph, set<int>& conj, State& state, Distancia& distancia, vector<int>& ordem, bool& prune, int rodada, bool primeiro_turno) {
	rotula(graph, distancia, state);

	vector<pair<int, int>> vertices;
	vector<bool> ja_colocado(nvertices, false);
	for (int vert : conj) {
		for (int adj : graph[vert]) {
			if (conj.find(adj) == conj.end() && state[adj] == Normal && !ja_colocado[adj]) {
				vertices.push_back({distancia[adj], adj});
				ja_colocado[adj] = true;
			}
		}
	}

	int menorDistancia;
	if (!vertices.empty()) {
		sort(vertices.begin(), vertices.end()); // Ordena pelas menores distâncias.
		menorDistancia = vertices[0].first;
	}
	else
		return conj.size();

	if (rodada >= menorDistancia) { // Não consegue defender.
		prune = true; // Se não dá pra proteger uma área A, não dá pra proteger uma área B que contenha A.
		return 0;
	}

	int i = 0;
	if (primeiro_turno)
		i = sequencia_final.size() % BRIGADISTAS_POR_TURNO; // Continua no turno (máximo 5 brigadistas).
	while (i < BRIGADISTAS_POR_TURNO && i < static_cast<int>(vertices.size())) {
		state[vertices[i].second] = Defendido;
		ordem.push_back(vertices[i].second);
		++i;
	}
	return pode_proteger(graph, conj, state, distancia, ordem, prune, rodada + 1, false);
}

void rotula(const Lista_Adj& graph, Distancia& distancia, State& state) { // Muda o vetor de distâncias de acordo com o de estados.
	queue<int> fila;
	fila.push(inicio);

	distancia.assign(nvertices, INT_MAX);
	distancia[inicio] = 0;
	vector<bool> marcado(nvertices, false);
	marcado[inicio] = true;

	while (!fila.empty()) { // BFS
		int atual = fila.front();
		fila.pop();
		for (int adj : graph[atual]) {
			if (!marcado[adj] && state[adj] == Normal) {
				marcado[adj] = true;
				fila.push(adj);
				distancia[adj] = distancia[atual] + 1;
			}
		}
	}
}

int testa_sequencia() {
	Lista_Adj graph(grafo);
	State state(nvertices, Normal);
	state[inicio] = Queimado;

	int i = 0;
	do { // Começa defendendo.
		if (sequencia_final.empty()) {
			defender_sequencialmente(BRIGADISTAS_POR_TURNO, state);
		}
		else {
			int usados = 0;
			for (int j = i; j < i + BRIGADISTAS_POR_TURNO && j < static_cast<int>(sequencia_final.size()); ++j) {
				if (state[sequencia_final[j]] == Normal) {
					state[sequencia_final[j]] = Defendido;
					usados++;
				}
			}

			assert(usados <= BRIGADISTAS_POR_TURNO);

			if (usados < BRIGADISTAS_POR_TURNO) {
				defender_sequencialmente(BRIGADISTAS_POR_TURNO - usados, state);
			}

			i += BRIGADISTAS_POR_TURNO;
		}
	} while (proximo_turno(graph, state));

	int salvos = 0, normais = 0;
	for (Estado vert : state) {
		if (vert != Queimado)
			salvos++;
		if (vert == Normal)
			normais++;
	}

	cout << "NORMAIS: " << normais << "\n";

	return salvos;
}

void defender_sequencialmente(int brigadistas, State &state){
	int i = 0, defended = 0, disponivel = brigadistas;
	while (i < brigadistas && i < static_cast<int>(state.size())) {
		if (state[i] == Normal && areas_finais.find(i) == areas_finais.end()) { // Não pertence à área defendida.
			state[i] = Defendido;
			defended++;
			sequencia_final.push_back(i);			
		}
		else 
			brigadistas++;
		++i;
	}
	assert(defended <= disponivel);
}

bool proximo_turno(Lista_Adj& graph, State& state) {
	bool queimou = false;
	for (int adj : graph[inicio]) {
		if (state[adj] == Normal) {
			state[adj] = Queimado;
			queimou = true;
		}
	}
	if (queimou)
		turn_count++;

	set<int> novos;
	for (int vert : graph[inicio]) {
		if (state[vert] == Queimado) { // Eventualmente pode estar defendido: não queimado no loop anterior.
			for (int adj : graph[vert])
				if (state[adj] == Normal)
					novos.insert(adj);
		}
	}
	graph[inicio].clear(); // Redefine os adjacentes ao vértice inicial.
	for (int vert : novos) {
		graph[inicio].push_back(vert);
		graph[vert].push_back(inicio);
	}

	return novos.size() > 0;
}

bool ja_foi_usado(set<int>& vertices){
	const string conjunto = area_to_string(vertices);
	if (tabu.find(conjunto) == tabu.end()) { // Não encontrou.
		tabu.insert(conjunto);
		return false;
	}
	return true;
}

const string area_to_string(set<int>& vertices) {
	string conjunto = "";
	for (int elem : vertices) {
		stringstream ss;
		ss << 'x' << elem;
		conjunto += ss.str();
	}
	return conjunto;
}