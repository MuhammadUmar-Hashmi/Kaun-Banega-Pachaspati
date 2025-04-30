#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <thread>
#include <chrono>
#include <cctype>
#include <curl/curl.h>
#include "nlohmann/json.hpp"
#include <windows.h> // For PlaySound
#include <mmsystem.h> // For sound functions
#pragma comment(lib, "winmm.lib")

using namespace std;
using json = nlohmann::json;

// Console Colors
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

void playStartupSound() {
    PlaySound(TEXT("start.wav"), NULL, SND_FILENAME | SND_ASYNC);
}

// --- Lifelines (Stack) ---
class Stack {
private:
    vector<string> items;
    bool used = false;
public:
    void push(const string& item) { items.push_back(item); }
    void pop() { if (!items.empty()) items.pop_back(); }
    string top() { return items.empty() ? "" : items.back(); }
    bool empty() { return items.empty() || used; }
    void markUsed() { used = true; }

    void display() {
        cout << CYAN << "- A) Ask a Friend" << RESET << endl;
        cout << CYAN << "- B) 50:50" << RESET << endl;
    }
};

// --- Question Queue ---
template<typename T>
class Queue {
private:
    vector<T> items;
public:
    void enqueue(const T& item) { items.push_back(item); }
    T dequeue() { T frontItem = items.front(); items.erase(items.begin()); return frontItem; }
    bool empty() { return items.empty(); }
};

// CURL Callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Decode HTML Entities
string decodeHtmlEntities(const string& input) {
    string output = input;
    vector<pair<string, string>> entities = {
        {"&quot;", "\""}, {"&#039;", "'"}, {"&amp;", "&"},
        {"&lt;", "<"}, {"&gt;", ">"}
    };
    for (const auto& [entity, replacement] : entities) {
        size_t pos = 0;
        while ((pos = output.find(entity, pos)) != string::npos) {
            output.replace(pos, entity.length(), replacement);
            pos += replacement.length();
        }
    }
    return output;
}

// Fetch Questions from API
json fetchQuestions() {
    CURL* curl;
    CURLcode res;
    string readBuffer;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://opentdb.com/api.php?amount=5&category=18&difficulty=medium&type=multiple");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return json::parse(readBuffer);
}

// Display Money Ladder
void displayMoneyLadder(const vector<int>& ladder, int current) {
    cout << BOLD << "\nMoney Ladder:\n" << RESET;
    for (int i = ladder.size() - 1; i >= 0; --i) {
        if (i == current)
            cout << GREEN << "->  Q" << i + 1 << ": Rs " << ladder[i] << RESET << endl;
        else
            cout << "    Q" << i + 1 << ": Rs " << ladder[i] << endl;
    }
    cout << endl;
}

// Ask a Question
bool askQuestion(const json& questionData, int qNumber, int& moneyWon, const vector<int>& moneyLadder, Stack& lifelines) {
    displayMoneyLadder(moneyLadder, qNumber);

    cout << YELLOW << "\nPress Enter to view the question..." << RESET;
    cin.ignore();
    cin.get();

    int timeLeft = 60;
    string userInput;
    random_device rd;
    default_random_engine rng(rd());

    vector<string> options = questionData["incorrect_answers"].get<vector<string>>();
    options.push_back(questionData["correct_answer"]);
    shuffle(options.begin(), options.end(), rng);

    cout << "\nTime Available: " << timeLeft << " seconds\n";
    cout << CYAN << "\nQuestion " << qNumber + 1 << " for Rs " << moneyLadder[qNumber] << ":\n" << RESET;
    cout << decodeHtmlEntities(questionData["question"]) << "\n\n";

    char labels[] = { 'A', 'B', 'C', 'D' };
    for (int i = 0; i < options.size(); ++i) {
        cout << labels[i] << ") " << decodeHtmlEntities(options[i]) << endl;
    }

    auto start = chrono::steady_clock::now();
    bool answered = false;

    while (true) {
        cout << "\nOptions: (Enter A/B/C/D), 'l' for Lifeline, 'quit' to exit.\nYour Choice: ";
        cin >> userInput;
        transform(userInput.begin(), userInput.end(), userInput.begin(), ::tolower);

        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(now - start).count();

        if (elapsed > timeLeft) {
            cout << RED << "\nTime's up! You are disqualified." << RESET << endl;
            cout << "You leave with Rs " << moneyWon << ".\n";
            exit(0);
        }

        if (userInput == "l") {
            if (lifelines.empty()) {
                cout << RED << "\nNo lifelines available!" << RESET << endl;
                continue;
            }
            cout << "\nAvailable Lifelines:\n";
            lifelines.display();
            cout << "Enter Lifeline Choice (A/B): ";
            string lifelineChoice;
            cin >> lifelineChoice;
            transform(lifelineChoice.begin(), lifelineChoice.end(), lifelineChoice.begin(), ::tolower);

            if (lifelineChoice == "b") {
                cout << YELLOW << "\nUsing 50:50 Lifeline!" << RESET << endl;
                timeLeft += 30;
                cout << "New Time: " << timeLeft << " seconds\n";
                vector<string> wrongOptions = questionData["incorrect_answers"];
                shuffle(wrongOptions.begin(), wrongOptions.end(), rng);
                vector<string> newOptions = { questionData["correct_answer"], wrongOptions[0] };
                shuffle(newOptions.begin(), newOptions.end(), rng);
                cout << "\nNew Options:\n";
                cout << "A) " << decodeHtmlEntities(newOptions[0]) << endl;
                cout << "B) " << decodeHtmlEntities(newOptions[1]) << endl;
                options = newOptions;
                lifelines.markUsed();
            }
            else if (lifelineChoice == "a") {
                cout << YELLOW << "\nCalling your friend... They say: 'Think carefully!'" << RESET << endl;
                timeLeft += 60;
                cout << "New Time: " << timeLeft << " seconds\n";
                lifelines.markUsed();
            }
            else {
                cout << RED << "\nInvalid Lifeline choice!" << RESET << endl;
            }
        }
        else if (userInput == "quit") {
            cout << "\nYou chose to quit. You won Rs " << moneyWon << ".\n";
            cout << GREEN << "\nCongratulations! You won Rs " << moneyWon << "!" << RESET << endl;
            exit(0);
        }
        else if (userInput.length() == 1 && (toupper(userInput[0]) >= 'A' && toupper(userInput[0]) <= 'D')) {
            answered = true;
            break;
        }
        else {
            cout << RED << "\nInvalid input. Try again." << RESET << endl;
        }
    }

    if (!answered) {
        cout << RED << "\nNo Answer! Disqualified." << RESET << endl;
        cout << "You leave with Rs " << moneyWon << ".\n";
        exit(0);
    }

    char selected = toupper(userInput[0]);
    int selectedIndex = selected - 'A';
    string selectedAnswer = options[selectedIndex];

    if (selectedAnswer == questionData["correct_answer"]) {
        cout << GREEN << "\nCorrect Answer! Moving to next question..." << RESET << endl;
        moneyWon = moneyLadder[qNumber];
        return true;
    }
    else {
        cout << RED << "\nWrong Answer!" << RESET << endl;
        cout << "Correct Answer was: " << decodeHtmlEntities(questionData["correct_answer"]) << endl;
        cout << GREEN << "\nYou won Rs 0." << RESET << endl;
        exit(0);
    }
}

int main() {
    playStartupSound();

    cout << CYAN << BOLD;
    cout << "========================================\n";
    cout << "         KAUN BANEGA Patchaspati          \n";
    cout << "        Presented by Umar and Ayan        \n";
    cout << "========================================\n" << RESET;

    json quizData = fetchQuestions();
    if (!quizData.contains("results")) {
        cout << RED << "Failed to fetch questions. Exiting..." << RESET << endl;
        return 1;
    }

    vector<int> moneyLadder = { 10, 20, 30, 40, 50 };
    int moneyWon = 0;

    Queue<json> questionsQueue;
    for (const auto& q : quizData["results"]) {
        questionsQueue.enqueue(q);
    }

    Stack lifelines;
    lifelines.push("Ask a Friend");
    lifelines.push("50:50");

    for (int i = 0; i < 5; ++i) {
        json question = questionsQueue.dequeue();
        bool correct = askQuestion(question, i, moneyWon, moneyLadder, lifelines);
    }

    cout << "\nCongratulations. You won Rs " << moneyWon << ".\n";
    return 0;
}









































// #include <iostream>
// #include <string>
// #include <vector>
// #include <algorithm>
// #include <random>
// #include<chrono>
// #include <cctype>
// #include <curl/curl.h>
// #include "nlohmann/json.hpp"

// using namespace std;
// using json = nlohmann::json;

// // Console Color Codes
// #define RESET   "\033[0m"
// #define RED     "\033[31m"
// #define GREEN   "\033[32m"
// #define YELLOW  "\033[33m"
// #define CYAN    "\033[36m"
// #define BOLD    "\033[1m"

// // --- Stack for Lifelines ---
// class Stack 
// {
//     private:
//         vector<string> items;
//         bool used = false; // Track if a lifeline has been used
//     public:
//         void push(const string& item) { items.push_back(item); }
//         void pop() { if (!items.empty()) items.pop_back(); }
//         string top() { return items.empty() ? "" : items.back(); }
//         bool empty() { return items.empty() || used; }
//         void markUsed() { used = true; }
        
//         void display() 
//         {
//             cout << CYAN << "- A) Call a Friend" << RESET << endl;
//             cout << CYAN << "- B) 50:50" << RESET << endl;
//         }
//     };

// // --- Queue for Questions ---
// template<typename T>
// class Queue {
// private:
//     vector<T> items;
// public:
//     void enqueue(const T& item) { items.push_back(item); }
//     T dequeue() { T frontItem = items.front(); items.erase(items.begin()); return frontItem; }
//     bool empty() { return items.empty(); }
// };

// static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) 
// {
//     ((string*)userp)->append((char*)contents, size * nmemb);
//     return size * nmemb;
// }

// string decodeHtmlEntities(const string& input) 
// {
//     string output = input;
//     vector<pair<string, string>> entities = 
//     {
//         {"&quot;", "\""}, {"&#039;", "'"}, {"&amp;", "&"},
//         {"&lt;", "<"}, {"&gt;", ">"}
//     };

//     for (const auto& [entity, replacement] : entities) 
//     {
//         size_t pos = 0;
//         while ((pos = output.find(entity, pos)) != string::npos) 
//         {
//             output.replace(pos, entity.length(), replacement);
//             pos += replacement.length();
//         }
//     }
//     return output;
// }

// json fetchQuestions() 
// {
//     CURL* curl;
//     CURLcode res;
//     string readBuffer;

//     curl_global_init(CURL_GLOBAL_DEFAULT);
//     curl = curl_easy_init();

//     if (curl) 
//     {
//         curl_easy_setopt(curl, CURLOPT_URL, "https://opentdb.com/api.php?amount=5&category=9&difficulty=hard&type=multiple");
//         curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
//         curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
//         res = curl_easy_perform(curl);
//         curl_easy_cleanup(curl);
//     }

//     curl_global_cleanup();

//     return json::parse(readBuffer);
// }

// void displayMoneyLadder(const vector<int>& ladder, int current) 
// {
//     cout << BOLD << "\nMoney Ladder:\n" << RESET;
//     for (int i = ladder.size() - 1; i >= 0; --i) 
//     {
//         if (i == current)
//             cout << GREEN << "->  Q" << i+1 << ": Rs " << ladder[i] << RESET << endl;
//         else
//             cout << "    Q" << i+1 << ": Rs " << ladder[i] << endl;
//     }
//     cout << endl;
// }

// bool askQuestion(const json& questionData, int qNumber, int& moneyWon, const vector<int>& moneyLadder, Stack& lifelines) 
// {

//     displayMoneyLadder(moneyLadder, qNumber);

//     cout << YELLOW << "\nPress Enter to view the question..." << RESET;
//     cin.ignore();
//     cin.get();

//     int timeLeft = 60;
//     string userInput;
//     random_device rd;
//     default_random_engine rng(rd());

//     vector<string> options = questionData["incorrect_answers"].get<vector<string>>();
//     options.push_back(questionData["correct_answer"]);
//     shuffle(options.begin(), options.end(), rng);

//     cout << "\nTime Available: " << timeLeft << " seconds\n";

//     cout << CYAN << "\nQuestion " << qNumber+1 << " for Rs " << moneyLadder[qNumber] << ":\n" << RESET;
//     cout << decodeHtmlEntities(questionData["question"]) << "\n\n";

//     char labels[] = {'A', 'B', 'C', 'D'};
//     for (int i = 0; i < options.size(); ++i) 
//     {
//         cout << labels[i] << ") " << decodeHtmlEntities(options[i]) << endl;
//     }

//     auto start = chrono::steady_clock::now();
//     bool answered = false;

//     while (true) 
//     {
//         cout << "\nOptions: (Enter A/B/C/D), 'l' for Lifeline, 'quit' to exit.\nYour Choice: ";
//         cin >> userInput;
//         transform(userInput.begin(), userInput.end(), userInput.begin(), ::tolower);

//         auto now = chrono::steady_clock::now();
//         auto elapsed = chrono::duration_cast<chrono::seconds>(now - start).count();

//         if (elapsed > timeLeft) {
//             cout << RED << "\nTime's up! You are disqualified." << RESET << endl;
//             cout << "You leave with Rs " << moneyWon << ".\n";
//             exit(0);
//         }

//         if (userInput == "l") 
//         {
//             if (lifelines.empty()) 
//             {
//                 cout << RED << "\nNo lifelines available!" << RESET << endl;
//                 continue;
//             }
//             cout << "\nAvailable Lifelines:\n";
//             lifelines.display();
//             cout << "Enter Lifeline Choice (A/B): ";
//             string lifelineChoice;
//             cin >> lifelineChoice;
//             transform(lifelineChoice.begin(), lifelineChoice.end(), lifelineChoice.begin(), ::tolower);

//             if (lifelineChoice == "b") 
//             {
//                 cout << YELLOW << "\nUsing 50:50 Lifeline!" << RESET << endl;
//                 timeLeft += 30;
//                 cout << "New Time: " << timeLeft << " seconds\n";
//                 vector<string> wrongOptions;
//                 for (const auto& opt : questionData["incorrect_answers"])
//                     wrongOptions.push_back(opt);
//                 shuffle(wrongOptions.begin(), wrongOptions.end(), rng);
//                 vector<string> newOptions = {questionData["correct_answer"], wrongOptions[0]};
//                 shuffle(newOptions.begin(), newOptions.end(), rng);
//                 cout << "\nNew Options:\n";
//                 cout << "A) " << decodeHtmlEntities(newOptions[0]) << endl;
//                 cout << "B) " << decodeHtmlEntities(newOptions[1]) << endl;
//                 options = newOptions;
//                 lifelines.markUsed(); // Lifeline used, block others
//             } 
//             else if (lifelineChoice == "a") 
//             {
//                 cout << YELLOW << "\nYou can ask a friend" << RESET << endl;
//                 timeLeft += 60;
//                 cout << "New Time: " << timeLeft << " seconds\n";
//                 lifelines.markUsed(); // Lifeline used, block others
//             } 
//             else 
//             {
//                 cout << RED << "\nInvalid Lifeline choice!" << RESET << endl;
//             }
//         }
//         else if (userInput == "quit") 
//         {
//             cout << "\nYou chose to quit. You won Rs " << moneyWon << ".\n";
//             cout << GREEN << "\nCongratulations! You won Rs " << moneyWon << "!" << RESET << endl;
//             exit(0);
//         }
//         else if (userInput.length() == 1 && (toupper(userInput[0]) >= 'A' && toupper(userInput[0]) <= 'D')) 
//         {
//             answered = true;
//             break;
//         } 
//         else 
//         {
//             cout << RED << "\nInvalid input. Try again." << RESET << endl;
//         }
//     }

//     if (!answered) 
//     {
//         cout << RED << "\nNo Answer! Disqualified." << RESET << endl;
//         cout << "You leave with Rs " << moneyWon << ".\n";
//         cout << GREEN << "\nCongratulations! You won Rs " << moneyWon << "!" << RESET << endl;
//         exit(0);
//     }

//     char selected = toupper(userInput[0]);
//     int selectedIndex = selected - 'A';
//     string selectedAnswer = options[selectedIndex];

//     if (selectedAnswer == questionData["correct_answer"]) 
//     {
//         cout << GREEN << "\nCorrect Answer! Moving to next question..." << RESET << endl;
//         moneyWon = moneyLadder[qNumber];
//         return true;
//     } 
//     else 
//     {
//         cout << RED << "\nWrong Answer!" << RESET << endl;
//         cout << "Correct Answer was: " << decodeHtmlEntities(questionData["correct_answer"]) << endl;
//         cout << GREEN << "\nCongratulations! You won Rs " << moneyWon << "!" << RESET << endl;
//         return false;
//     }
// }


// int main() 
// {
//     cout << CYAN << BOLD;
//     cout << "========================================\n";
//     cout << "        KAUN BANEGA PACHASPATI           \n";
//     cout << "      Presented by Umar and Ayan             \n";
//     cout << "========================================\n" << RESET;

//     json quizData = fetchQuestions();
//     if (!quizData.contains("results")) 
//     {
//         cout << RED << "Failed to fetch questions. Exiting..." << RESET << endl;
//         return 1;
//     }

//     vector<int> moneyLadder = {10, 20, 30, 40, 50};
//     int moneyWon = 0;

//     Queue<json> questionsQueue;
//     for (const auto& q : quizData["results"]) 
//     {
//         questionsQueue.enqueue(q);
//     }

//     Stack lifelines;
//     lifelines.push("Ask a Friend");
//     lifelines.push("50:50");

//     for (int i = 0; i < 5; ++i) {
//         json question = questionsQueue.dequeue();
//         bool correct = askQuestion(question, i, moneyWon, moneyLadder, lifelines);
//         if (!correct) {
//             exit(0);
//         }
//     }
    

//     cout << "\nCongratulations. You won Rs " << moneyWon << ".\n";
//     return 0;
// }
