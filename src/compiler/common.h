﻿/*🍲Ketl🍲*/
#ifndef compiler_common_h
#define compiler_common_h

#include "ketl.h"
#include "type.h"

namespace Ketl {

	struct StringHash {
		using is_transparent = std::true_type;

		std::size_t operator()(const std::string_view& str) const noexcept
		{
			return std::hash<std::string_view>()(str);
		}
	};

	struct StringEqualTo {
		using is_transparent = std::true_type;

		bool operator()(const std::string_view& lhs, const std::string_view& rhs) const noexcept
		{
			return lhs == rhs;
		}
	};

	class Context;

	inline OperatorCode parseOperatorCode(const std::string_view& opStr) {
		switch (opStr.length()) {
		case 1: {
			switch (opStr.data()[0]) {
			case '+': return OperatorCode::Plus;
			case '-': return OperatorCode::Minus;
			case '*': return OperatorCode::Multiply;
			case '/': return OperatorCode::Divide;
			case '=': return OperatorCode::Assign;
			}
		}
		}

		return OperatorCode::None;
	}


	class TypeObject;
	class Context;

	class Variable {
	public:

		Variable() {}
		Variable(void* data, const TypeObject& type)
			: _data(data), _type(&type) {}

		void* as(std::type_index typeIndex, Context& context) const;

		const TypeObject& type() const {
			return *_type;
		}

		void* rawData() const {
			return _data;
		}

		void data(void* data) {
			_data = data;
		}

	private:

		friend class Context;

		void* _data = nullptr;
		const TypeObject* _type = nullptr;

	};

}

#endif /*compiler_common_h*/