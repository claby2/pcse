#ifndef LEXER_HPP
#define LEXER_HPP

#include <string>
#include <vector>
#include <cstdlib>
#include <map>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

#include <cstring>

#include "fraction.hpp"
#include "utils.hpp"

// Token list {{{

// not allowed to put directly

// We will use X-Macros
// to make the enum <-> string conversion easier.
#define TOKENTYPE_LIST \
	TOK(LEFT_PAREN) \
	TOK(RIGHT_PAREN) \
	TOK(LEFT_SQ) \
	TOK(RIGHT_SQ) \
	TOK(COMMA) \
	TOK(MINUS) \
	TOK(PLUS) \
	TOK(SLASH) \
	TOK(STAR) \
	TOK(COLON) \
	TOK(ASSIGN) \
	TOK(EQ) \
	TOK(LT_GT) \
	TOK(GT) \
	TOK(GT_EQ) \
	TOK(LT) \
	TOK(LT_EQ) \
	TOK(AND) RESERVED(AND)\
	TOK(OR) RESERVED(OR)\
	TOK(NOT) RESERVED(NOT)\
	TOK(IF)	RESERVED(IF)\
	TOK(THEN)	RESERVED(THEN)\
	TOK(ELSE)	RESERVED(ELSE)\
	TOK(ENDIF)	RESERVED(ENDIF)\
	TOK(DECLARE)	RESERVED(DECLARE)\
	TOK(FOR)	RESERVED(FOR)\
	TOK(TO)	RESERVED(TO)\
	TOK(STEP)	RESERVED(STEP)\
	TOK(NEXT)	RESERVED(NEXT)\
	TOK(WHILE)	RESERVED(WHILE)\
	TOK(ENDWHILE)	RESERVED(ENDWHILE)\
	TOK(REPEAT)	RESERVED(REPEAT)\
	TOK(UNTIL)	RESERVED(UNTIL)\
	TOK(CONSTANT)	RESERVED(CONSTANT)\
	TOK(INPUT)	RESERVED(INPUT)\
	TOK(OUTPUT)	RESERVED(OUTPUT)\
	TOK(CASE)	RESERVED(CASE)\
	TOK(OF)	RESERVED(OF)\
	TOK(OTHERWISE)	RESERVED(OTHERWISE)\
	TOK(ENDCASE)	RESERVED(ENDCASE)\
	TOK(PROCEDURE)	RESERVED(PROCEDURE)\
	TOK(BYREF)	RESERVED(BYREF)\
	TOK(ENDPROCEDURE)	RESERVED(ENDPROCEDURE)\
	TOK(CALL)	RESERVED(CALL)\
	TOK(FUNCTION)	RESERVED(FUNCTION)\
	TOK(RETURNS)	RESERVED(RETURNS)\
	TOK(RETURN)	RESERVED(RETURN)\
	TOK(ENDFUNCTION)	RESERVED(ENDFUNCTION)\
	TOK(INTEGER) RESERVED(INTEGER)\
	TOK(REAL) RESERVED(REAL)\
	TOK(STRING) RESERVED(STRING)\
	TOK(ARRAY) RESERVED(ARRAY)\
	TOK(CHAR) RESERVED(CHAR)\
	TOK(BOOLEAN) RESERVED(BOOLEAN)\
	TOK(DATE) RESERVED(DATE)\
	TOK(TRUE) RESERVED(TRUE)\
	TOK(FALSE) RESERVED(FALSE)\
	TOK(MOD) RESERVED(MOD)\
	TOK(DIV) RESERVED(DIV)\
	/* special */ \
	TOK(IDENTIFIER)\
	TOK(STR_C)\
	TOK(INT_C)\
	TOK(REAL_C)\
	TOK(INVALID)

// }}}

// TokenType data structures {{{

/* default */
#define RESERVED(a) /* nothing */

enum class TokenType {
#define TOK(a) a,
	TOKENTYPE_LIST
#undef TOK
	LENGTH
};

const size_t TOKENTYPE_LENGTH = static_cast<int>(TokenType::LENGTH);

const std::vector<std::string_view> TokenTypeStrTable = {
#define TOK(a) #a,
	TOKENTYPE_LIST
#undef TOK
};

inline std::string_view tokenTypeToStr(const TokenType type) noexcept {
	return TokenTypeStrTable[static_cast<int>(type)];
}


const std::map<std::string_view, TokenType> reservedWords {
#define TOK(a) /* nothing */
#undef RESERVED
#define RESERVED(a) { #a, TokenType:: a },
	TOKENTYPE_LIST
#undef RESERVED
#define RESERVED(a) /* nothing */
};

const std::vector<bool> is_reserved_word = [](){
	std::vector<bool> res(TOKENTYPE_LENGTH, false);
	for(const auto& x : reservedWords){
		res[static_cast<int>(x.second)] = true;
	}
	return res;
}();

inline bool isReservedWord(const TokenType type) noexcept {
	return is_reserved_word[static_cast<int>(type)];
}

const std::string MAX_FRAC_NUM_STR = std::to_string(std::numeric_limits<Fraction<>::num_type>::max());
const std::string MAX_INT_STR = std::to_string(std::numeric_limits<int64_t>::max());

// }}}

// Token {{{

struct Token {
	const size_t line, col;
	const TokenType type;
	const union Literal {
		std::string_view str;
		int64_t i64;
		Fraction<> frac;
		Literal(std::string_view str_): str(str_) {}
		Literal(int64_t i): i64(i) {}
		Literal(int i): i64(i) {}
		Literal(Fraction<> frac_): frac(frac_) {}
		Literal(const char *p) : str(p) {}
	} literal;
	Token(size_t line_, size_t col_, TokenType type_, Literal lit_) :
		line(line_), col(col_), type(type_), literal(lit_) {}
	bool operator==(const Token& other) const noexcept {
		return line == other.line
			&& col == other.col
			&& type == other.type
			&& (type == TokenType::REAL_C ? literal.frac == other.literal.frac :
				type == TokenType::INT_C || type == TokenType::IDENTIFIER ? literal.i64 == other.literal.i64 :
				type == TokenType::STR_C ? literal.str == other.literal.str :
				/* has to be a reserved word or token, so is true */ true);
	}
	/* Make Catch2 print our type */
	friend std::ostream& operator<<(std::ostream& os, const Token& tok) noexcept {
		os << "{ line = " << tok.line
			<< ", col = " << tok.col
			<< ", type = " << tokenTypeToStr(tok.type) 
			<< ", literal";
		if(tok.type == TokenType::REAL_C){
			os << ".frac = " << tok.literal.frac;
		
		} else if(tok.type == TokenType::STR_C){
			// Str
			os << ".str = " << tok.literal.str;
		} else {
			// tok.type == TokenType::INT_C || t.type == TokenType::IDENTIFIER or is reserved word
			os << ".i64 = " << tok.literal.i64;
		}
		os << "}\n";
		return os;
	}
};

// }}}

// Lexer {{{

class LexError : public std::runtime_error {
public:
	size_t pos, line, col;
	template<typename T>
	LexError(size_t pos_, size_t line_, size_t col_, const T msg): std::runtime_error(msg), pos(pos_), line(line_), col(col_) {}
};

class Lexer {
public:
	std::string_view source;
	std::vector<Token> output;
	std::vector<size_t> line_loc;
protected:
	int64_t identifier_count = 1;
	std::map<std::string_view, int64_t> id_num;
	size_t line = 1;
	size_t curr = 0;

	// done()/peek()/next() like functions {{{
	inline size_t getCol() const noexcept {
		return (line_loc.empty() ? curr : curr - line_loc.back() - 1);
	}
	size_t getCol(size_t pos) const noexcept {
		if(line_loc.empty()) return pos + 1;
		// most common case
		else if(pos >= line_loc.back()) return pos - line_loc.back();
		else {
			// Otherwise, we have to find the last '\n' that has position <= pos.
			auto lastnl = std::upper_bound(line_loc.begin(), line_loc.end(), pos);
			if(lastnl == line_loc.begin()){
				// was before any newline
				return pos + 1;
			} else {
				--lastnl;
				return pos - *lastnl;
			}
		}
	}
	inline void emit(TokenType type) noexcept {
		output.emplace_back(line, getCol(), type, 0);
	}
	inline void emit(TokenType type, Token::Literal lt) noexcept {
		output.emplace_back(line, getCol(), type, lt);
	}
	inline void emit(TokenType type, Token::Literal lt, size_t startpos) noexcept {
		output.emplace_back(line, getCol(startpos), type, lt);
	}
	inline bool done() const noexcept {
		return curr >= source.length();
	}
	inline char peek() const noexcept {
		return done() ? '\0' : source[curr];
	}
	inline char next() noexcept {
		char c = peek();
		++curr;
		return c;
	}
	inline bool match(const char c) noexcept {
		if(c == peek()){
			next();
			return true;
		} else return false;
	}
	template<typename T>
	inline void error(const T msg){
		throw LexError(curr-1, line, getCol(), msg);
	}
	inline void expect(char c){
		if(c != next()){
			std::string msg = "Expected ";
			msg += c;
			error(msg);
		}
	}
	// }}}
	
	// newline, number, string, identifier {{{
	inline void newline() noexcept {
		line_loc.push_back(curr - 1);
		line++;
	}
	inline void number(){
		// Integer or Real
		const size_t start = curr-1;
		while(isDigit(peek())) next();
		if(peek() == '.'){
			// Real
			next();
			if(!isDigit(peek())){
				error("Expected digit after decimal point");
			}
			next();
			while(isDigit(peek())) next();
			if(curr - start >= MAX_FRAC_NUM_STR.length()){
				error("Real constant too large");
			}
			if(isAlpha(peek())){
				// We won't allow 12.2e2.
				// Makes for confusion.
				next();
				error("Unexpected character after number");
			}
			// parse fraction
			int32_t top, bot;
			{
				int32_t mul = 1;
				top = 0;
				for(size_t i = curr-1; i >= start; i--){
					if(source[i] == '.'){
						bot = mul;
					} else {
						top += (int32_t)(source[i] - '0') * mul;
						mul *= 10;
					}
				}
			}
			emit(TokenType::REAL_C, Fraction<>(top, bot), start);
		} else {
			// Integer
			if(isAlpha(peek())){
				// 12e2 is not allowed.
				next();
				error("Unexpected character after number");
			}
			// check if is too big
			// (stupid check so that math using it doesn't overflow easily)
			if(curr - start >= MAX_INT_STR.length()){
				error("Integer constant too large");
			}
			// parse integer
			int64_t res = 0;
			{
				int64_t mul = 1;
				for(size_t i = curr-1; i >= start; i--){
					res += (int64_t)(source[i] - '0') * mul;
					mul *= 10;
				}
			}
			emit(TokenType::INT_C, res, start);
		}
	}
	inline void string(){
		const size_t start = curr - 1;
		while(!done() && peek() != '"'){
			if(next() == '\n') newline();
		}
		// will throw if the string is incomplete
		expect('"');
		// remove first and last (`"str"` => `str`)
		emit(TokenType::STR_C, source.substr(start+1, curr - start-2), start);
	}
	inline void identifier(){
		const size_t start = curr-1;
		while(isAlpha(peek()) || peek() == '_') next();
		std::string_view id = source.substr(start, curr - start);
		if(reservedWords.find(id) != reservedWords.end()){
			emit(reservedWords.at(id), 0, start);
		} else {
			int64_t idn = identifier_count;
			if(id_num.find(id) != id_num.end()){
				idn = id_num[id];
			} else {
				id_num[id] = identifier_count;
				identifier_count++;
			}
			emit(TokenType::IDENTIFIER, idn, start);
		}
	}
	// }}}

	// lex {{{	
	void lex(){
		while(!done()){
			char c = next();
			switch(c){
				case '(': emit(TokenType::LEFT_PAREN); break;
				case ')': emit(TokenType::RIGHT_PAREN); break;
				case '[': emit(TokenType::LEFT_SQ); break;
				case ']': emit(TokenType::RIGHT_SQ); break;
				case ',': emit(TokenType::COMMA); break;
				case '-': emit(TokenType::MINUS); break;
				case '+': emit(TokenType::PLUS); break;
				case '/': 
					if(match('/')){
						// comment
						while(peek() != '\n') next();
						next();
						newline();
					} else {
						emit(TokenType::SLASH);
					}
					break;
				case '*': emit(TokenType::STAR); break;
				case ':': emit(TokenType::COLON); break;
				case '=': emit(TokenType::EQ); break;
				case '<':
					if(match('-')) emit(TokenType::ASSIGN, 0, curr - 2);
					else if(match('=')) emit(TokenType::LT_EQ, 0, curr - 2);
					else if(match('>')) emit(TokenType::LT_GT, 0, curr - 2);
					else emit(TokenType::LT);
					break;
				case '>':
					if(match('=')) emit(TokenType::GT_EQ, 0, curr - 2);
					else emit(TokenType::GT);
					break;
				case ' ':
				case '\r':
				case '\t':
					// ignore whitespace
					break;
				case '\n':
					newline();
					break;
				case '"':
					string();
					break;
				default:
					if(isDigit(c))
						number();
					else if(isAlpha(c))
						identifier();
					else {
						std::string msg = "Stray ";
						msg += c;
						msg += " in program";
						error(msg);
					}
					break;

			}
		}
	}
	// }}}
public:
	inline Lexer(const std::string_view source_) : source(source_) { lex(); }
};

// }}}

#endif /* LEXER_HPP */