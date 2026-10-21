// Library Management System
// Concepts: classes, arrays, vectors, hand-built linked list,
// file I/O for persistence, a menu loop, input validation
//
// Build:  g++ -std=c++17 -Wall -o library library.cpp
// Run:    ./library
// Data is saved to books.txt and members.txt in the current folder.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

const int MAX_LOANS = 3;  // each member can borrow up to 3 books 

// ---------------------------------------------------------------
// Book: stored in a std::vector
// ---------------------------------------------------------------
struct Book {
    int id;
    string title;
    string author;
    int borrowerId;  // 0 means "on the shelf"

    bool available() const { return borrowerId == 0; }
};

// ---------------------------------------------------------------
// Member: holds a fixed-size ARRAY of the book IDs they've borrowed
// ---------------------------------------------------------------
struct Member {
    int id;
    string name;
    int loans[MAX_LOANS];  // book IDs, 0 = empty slot
    int loanCount;

    Member(int id_, const string& name_) : id(id_), name(name_), loanCount(0) {
        for (int i = 0; i < MAX_LOANS; i++) loans[i] = 0;
    }

    bool addLoan(int bookId) {
        for (int i = 0; i < MAX_LOANS; i++) {
            if (loans[i] == 0) {
                loans[i] = bookId;
                loanCount++;
                return true;
            }
        }
        return false;  // no free slot
    }

    void removeLoan(int bookId) {
        for (int i = 0; i < MAX_LOANS; i++) {
            if (loans[i] == bookId) {
                loans[i] = 0;
                loanCount--;
                return;
            }
        }
    }
};

// ---------------------------------------------------------------
// MemberList: a singly LINKED LIST built by hand with new/delete
// ---------------------------------------------------------------
struct Node {
    Member data;
    Node* next;
    Node(const Member& m) : data(m), next(nullptr) {}
};

class MemberList {
    Node* head = nullptr;

public:
    ~MemberList() { clear(); }

    // Copying a list of raw pointers is risky, so we forbid it.
    MemberList() = default;
    MemberList(const MemberList&) = delete;
    MemberList& operator=(const MemberList&) = delete;

    void clear() {
        while (head) {
            Node* temp = head;
            head = head->next;
            delete temp;  // free every node: no memory leaks
        }
    }

    void add(const Member& m) {
        Node* node = new Node(m);
        if (!head) {
            head = node;
            return;
        }
        Node* cur = head;
        while (cur->next) cur = cur->next;  // walk to the tail
        cur->next = node;
    }

    Member* find(int id) {
        for (Node* cur = head; cur; cur = cur->next)
            if (cur->data.id == id) return &cur->data;
        return nullptr;
    }

    bool remove(int id) {
        Node* prev = nullptr;
        for (Node* cur = head; cur; prev = cur, cur = cur->next) {
            if (cur->data.id == id) {
                if (prev) prev->next = cur->next;
                else head = cur->next;
                delete cur;
                return true;
            }
        }
        return false;
    }

    // Lets Library loop over members without exposing Node
    template <typename Func>
    void forEach(Func f) {
        for (Node* cur = head; cur; cur = cur->next) f(cur->data);
    }

    bool empty() const { return head == nullptr; }
};

// ---------------------------------------------------------------
// Input helpers: the "robust error handling" part
// ---------------------------------------------------------------
string readLine(const string& prompt) {
    string line;
    while (true) {
        cout << prompt;
        if (!getline(cin, line)) {  // Ctrl+D / end of input
            cout << "\nInput closed. Exiting.\n";
            exit(0);
        }
        if (!line.empty()) return line;
        cout << "  Please enter something.\n";
    }
}

int readInt(const string& prompt, int minVal, int maxVal) {
    while (true) {
        string line = readLine(prompt);
        try {
            size_t used = 0;
            int value = stoi(line, &used);
            if (used != line.size()) throw invalid_argument("trailing chars");
            if (value < minVal || value > maxVal) {
                cout << "  Enter a number between " << minVal << " and " << maxVal << ".\n";
                continue;
            }
            return value;
        } catch (const exception&) {
            cout << "  That's not a valid number. Try again.\n";
        }
    }
}

string lower(string s) {
    for (char& c : s) c = tolower(static_cast<unsigned char>(c));
    return s;
}

// Strip '|' so it can't break our file format
string clean(string s) {
    for (char& c : s)
        if (c == '|') c = '/';
    return s;
}

// ---------------------------------------------------------------
// Library: ties everything together
// ---------------------------------------------------------------
class Library {
    vector<Book> books;
    MemberList members;
    int nextBookId = 1;
    int nextMemberId = 1;

    const string BOOK_FILE = "books.txt";
    const string MEMBER_FILE = "members.txt";

    Book* findBook(int id) {
        for (Book& b : books)
            if (b.id == id) return &b;
        return nullptr;
    }

    // ---- File persistence ----
    // books.txt   : id|title|author|borrowerId
    // members.txt : id|name|loan1,loan2,loan3
    void save() {
        ofstream bout(BOOK_FILE);
        for (const Book& b : books)
            bout << b.id << '|' << b.title << '|' << b.author << '|' << b.borrowerId << '\n';

        ofstream mout(MEMBER_FILE);
        members.forEach([&](Member& m) {
            mout << m.id << '|' << m.name << '|';
            for (int i = 0; i < MAX_LOANS; i++) mout << m.loans[i] << (i < MAX_LOANS - 1 ? "," : "");
            mout << '\n';
        });

        if (!bout || !mout) cout << "  Warning: could not write data files!\n";
    }

    void load() {
        ifstream bin(BOOK_FILE);
        string line;
        while (getline(bin, line)) {
            stringstream ss(line);
            string id, title, author, borrower;
            if (getline(ss, id, '|') && getline(ss, title, '|') &&
                getline(ss, author, '|') && getline(ss, borrower)) {
                try {
                    Book b{stoi(id), title, author, stoi(borrower)};
                    books.push_back(b);
                    if (b.id >= nextBookId) nextBookId = b.id + 1;
                } catch (...) {
                    cout << "  Skipped a corrupted line in " << BOOK_FILE << "\n";
                }
            }
        }

        ifstream min(MEMBER_FILE);
        while (getline(min, line)) {
            stringstream ss(line);
            string id, name, loanStr;
            if (getline(ss, id, '|') && getline(ss, name, '|') && getline(ss, loanStr)) {
                try {
                    Member m(stoi(id), name);
                    stringstream ls(loanStr);
                    string item;
                    while (getline(ls, item, ',')) {
                        int bookId = stoi(item);
                        if (bookId != 0) m.addLoan(bookId);
                    }
                    members.add(m);
                    if (m.id >= nextMemberId) nextMemberId = m.id + 1;
                } catch (...) {
                    cout << "  Skipped a corrupted line in " << MEMBER_FILE << "\n";
                }
            }
        }
    }

public:
    Library() { load(); }

    // ---- Books ----
    void addBook() {
        string title = clean(readLine("Title: "));
        string author = clean(readLine("Author: "));
        books.push_back({nextBookId, title, author, 0});
        cout << "  Added \"" << title << "\" with ID " << nextBookId << ".\n";
        nextBookId++;
        save();
    }

    void listBooks() {
        if (books.empty()) {
            cout << "  No books yet.\n";
            return;
        }
        cout << "\n  ID   Title                          Author               Status\n";
        cout << "  ---- ------------------------------ -------------------- ----------------\n";
        for (const Book& b : books) {
            string status = b.available() ? "Available" : "Borrowed (member " + to_string(b.borrowerId) + ")";
            printf("  %-4d %-30.30s %-20.20s %s\n", b.id, b.title.c_str(), b.author.c_str(), status.c_str());
        }
    }

    void searchBooks() {
        string term = lower(readLine("Search title or author: "));
        bool found = false;
        for (const Book& b : books) {
            if (lower(b.title).find(term) != string::npos || lower(b.author).find(term) != string::npos) {
                cout << "  [" << b.id << "] " << b.title << " by " << b.author
                     << (b.available() ? " - Available" : " - Borrowed") << "\n";
                found = true;
            }
        }
        if (!found) cout << "  No matches.\n";
    }

    void removeBook() {
        int id = readInt("Book ID to remove: ", 1, 1000000);
        for (size_t i = 0; i < books.size(); i++) {
            if (books[i].id == id) {
                if (!books[i].available()) {
                    cout << "  Can't remove a book that is currently borrowed.\n";
                    return;
                }
                cout << "  Removed \"" << books[i].title << "\".\n";
                books.erase(books.begin() + i);
                save();
                return;
            }
        }
        cout << "  No book with ID " << id << ".\n";
    }

    // ---- Members ----
    void addMember() {
        string name = clean(readLine("Member name: "));
        members.add(Member(nextMemberId, name));
        cout << "  Added " << name << " with member ID " << nextMemberId << ".\n";
        nextMemberId++;
        save();
    }

    void listMembers() {
        if (members.empty()) {
            cout << "  No members yet.\n";
            return;
        }
        cout << "\n  ID   Name                 Books borrowed\n";
        cout << "  ---- -------------------- ---------------------------\n";
        members.forEach([&](Member& m) {
            string titles;
            for (int i = 0; i < MAX_LOANS; i++) {
                if (m.loans[i] != 0) {
                    Book* b = findBook(m.loans[i]);
                    if (!titles.empty()) titles += "; ";
                    titles += b ? b->title : "(missing book)";
                }
            }
            if (titles.empty()) titles = "-";
            printf("  %-4d %-20.20s %s\n", m.id, m.name.c_str(), titles.c_str());
        });
    }

    void removeMember() {
        int id = readInt("Member ID to remove: ", 1, 1000000);
        Member* m = members.find(id);
        if (!m) {
            cout << "  No member with ID " << id << ".\n";
            return;
        }
        if (m->loanCount > 0) {
            cout << "  " << m->name << " still has books out. Return them first.\n";
            return;
        }
        cout << "  Removed " << m->name << ".\n";
        members.remove(id);
        save();
    }

    // ---- Checkout / return ----
    void checkOut() {
        int memberId = readInt("Member ID: ", 1, 1000000);
        Member* m = members.find(memberId);
        if (!m) {
            cout << "  No member with ID " << memberId << ".\n";
            return;
        }
        int bookId = readInt("Book ID: ", 1, 1000000);
        Book* b = findBook(bookId);
        if (!b) {
            cout << "  No book with ID " << bookId << ".\n";
            return;
        }
        if (!b->available()) {
            cout << "  \"" << b->title << "\" is already borrowed.\n";
            return;
        }
        if (!m->addLoan(bookId)) {
            cout << "  " << m->name << " already has " << MAX_LOANS << " books (the limit).\n";
            return;
        }
        b->borrowerId = memberId;
        cout << "  " << m->name << " checked out \"" << b->title << "\".\n";
        save();
    }

    void returnBook() {
        int bookId = readInt("Book ID being returned: ", 1, 1000000);
        Book* b = findBook(bookId);
        if (!b) {
            cout << "  No book with ID " << bookId << ".\n";
            return;
        }
        if (b->available()) {
            cout << "  That book isn't checked out.\n";
            return;
        }
        Member* m = members.find(b->borrowerId);
        if (m) m->removeLoan(bookId);
        cout << "  \"" << b->title << "\" returned" << (m ? " by " + m->name : "") << ".\n";
        b->borrowerId = 0;
        save();
    }
};

// ---------------------------------------------------------------
// Menu loop
// ---------------------------------------------------------------
void printMenu() {
    cout << "\n===== LIBRARY MENU =====\n"
         << " 1. Add book\n"
         << " 2. List books\n"
         << " 3. Search books\n"
         << " 4. Remove book\n"
         << " 5. Add member\n"
         << " 6. List members\n"
         << " 7. Remove member\n"
         << " 8. Check out a book\n"
         << " 9. Return a book\n"
         << " 0. Exit\n";
}

int main() {
    Library lib;
    cout << "Welcome to the Library Management System!\n";

    while (true) {
        printMenu();
        int choice = readInt("Choose an option: ", 0, 9);
        switch (choice) {
            case 1: lib.addBook(); break;
            case 2: lib.listBooks(); break;
            case 3: lib.searchBooks(); break;
            case 4: lib.removeBook(); break;
            case 5: lib.addMember(); break;
            case 6: lib.listMembers(); break;
            case 7: lib.removeMember(); break;
            case 8: lib.checkOut(); break;
            case 9: lib.returnBook(); break;
            case 0:
                cout << "Goodbye! Your data has been saved.\n";
                return 0;
        }
    }
}