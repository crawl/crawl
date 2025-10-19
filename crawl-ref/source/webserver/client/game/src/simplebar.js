/**
 * SimpleBar.js - v3.0.0-beta.4
 * Scrollbars, simpler.
 * https://grsmto.github.io/simplebar/
 *
 * Includes the following embedded node package dependencies:
 *  - can-use-dom
 *  - lodash-throttle
 *  - resize-observer-polyfill
 *  - scrollbarwidth
 *
 * Made by Adrien Denat from a fork by Jonathan Nicol
 * Under MIT License
 */

(function (global, factory) {
  typeof exports === 'object' && typeof module !== 'undefined' ? module.exports = factory() :
  typeof define === 'function' && define.amd ? define(factory) :
  (global.SimpleBar = factory());
}(this, (function () { 'use strict';

  var _isObject = function (it) {
    return typeof it === 'object' ? it !== null : typeof it === 'function';
  };

  var _anObject = function (it) {
    if (!_isObject(it)) throw TypeError(it + ' is not an object!');
    return it;
  };

  var _fails = function (exec) {
    try {
      return !!exec();
    } catch (e) {
      return true;
    }
  };

  // Thank's IE8 for his funny defineProperty
  var _descriptors = !_fails(function () {
    return Object.defineProperty({}, 'a', { get: function () { return 7; } }).a != 7;
  });

  var commonjsGlobal = typeof window !== 'undefined' ? window : typeof global !== 'undefined' ? global : typeof self !== 'undefined' ? self : {};

  function createCommonjsModule(fn, module) {
    return module = { exports: {} }, fn(module, module.exports), module.exports;
  }

  var _global = createCommonjsModule(function (module) {
  // https://github.com/zloirock/core-js/issues/86#issuecomment-115759028
  var global = module.exports = typeof window != 'undefined' && window.Math == Math
    ? window : typeof self != 'undefined' && self.Math == Math ? self
    // eslint-disable-next-line no-new-func
    : Function('return this')();
  if (typeof __g == 'number') __g = global; // eslint-disable-line no-undef
  });

  var document$1 = _global.document;
  // typeof document.createElement is 'object' in old IE
  var is = _isObject(document$1) && _isObject(document$1.createElement);
  var _domCreate = function (it) {
    return is ? document$1.createElement(it) : {};
  };

  var _ie8DomDefine = !_descriptors && !_fails(function () {
    return Object.defineProperty(_domCreate('div'), 'a', { get: function () { return 7; } }).a != 7;
  });

  // 7.1.1 ToPrimitive(input [, PreferredType])

  // instead of the ES6 spec version, we didn't implement @@toPrimitive case
  // and the second argument - flag - preferred type is a string
  var _toPrimitive = function (it, S) {
    if (!_isObject(it)) return it;
    var fn, val;
    if (S && typeof (fn = it.toString) == 'function' && !_isObject(val = fn.call(it))) return val;
    if (typeof (fn = it.valueOf) == 'function' && !_isObject(val = fn.call(it))) return val;
    if (!S && typeof (fn = it.toString) == 'function' && !_isObject(val = fn.call(it))) return val;
    throw TypeError("Can't convert object to primitive value");
  };

  var dP = Object.defineProperty;

  var f = _descriptors ? Object.defineProperty : function defineProperty(O, P, Attributes) {
    _anObject(O);
    P = _toPrimitive(P, true);
    _anObject(Attributes);
    if (_ie8DomDefine) try {
      return dP(O, P, Attributes);
    } catch (e) { /* empty */ }
    if ('get' in Attributes || 'set' in Attributes) throw TypeError('Accessors not supported!');
    if ('value' in Attributes) O[P] = Attributes.value;
    return O;
  };

  var _objectDp = {
    f: f
  };

  var _propertyDesc = function (bitmap, value) {
    return {
      enumerable: !(bitmap & 1),
      configurable: !(bitmap & 2),
      writable: !(bitmap & 4),
      value: value
    };
  };

  var _hide = _descriptors ? function (object, key, value) {
    return _objectDp.f(object, key, _propertyDesc(1, value));
  } : function (object, key, value) {
    object[key] = value;
    return object;
  };

  var hasOwnProperty = {}.hasOwnProperty;
  var _has = function (it, key) {
    return hasOwnProperty.call(it, key);
  };

  var id = 0;
  var px = Math.random();
  var _uid = function (key) {
    return 'Symbol('.concat(key === undefined ? '' : key, ')_', (++id + px).toString(36));
  };

  var _core = createCommonjsModule(function (module) {
  var core = module.exports = { version: '2.5.7' };
  if (typeof __e == 'number') __e = core; // eslint-disable-line no-undef
  });
  var _core_1 = _core.version;

  var _redefine = createCommonjsModule(function (module) {
  var SRC = _uid('src');
  var TO_STRING = 'toString';
  var $toString = Function[TO_STRING];
  var TPL = ('' + $toString).split(TO_STRING);

  _core.inspectSource = function (it) {
    return $toString.call(it);
  };

  (module.exports = function (O, key, val, safe) {
    var isFunction = typeof val == 'function';
    if (isFunction) _has(val, 'name') || _hide(val, 'name', key);
    if (O[key] === val) return;
    if (isFunction) _has(val, SRC) || _hide(val, SRC, O[key] ? '' + O[key] : TPL.join(String(key)));
    if (O === _global) {
      O[key] = val;
    } else if (!safe) {
      delete O[key];
      _hide(O, key, val);
    } else if (O[key]) {
      O[key] = val;
    } else {
      _hide(O, key, val);
    }
  // add fake Function#toString for correct work wrapped methods / constructors with methods like LoDash isNative
  })(Function.prototype, TO_STRING, function toString() {
    return typeof this == 'function' && this[SRC] || $toString.call(this);
  });
  });

  // 7.2.1 RequireObjectCoercible(argument)
  var _defined = function (it) {
    if (it == undefined) throw TypeError("Can't call method on  " + it);
    return it;
  };

  var _library = false;

  var _shared = createCommonjsModule(function (module) {
  var SHARED = '__core-js_shared__';
  var store = _global[SHARED] || (_global[SHARED] = {});

  (module.exports = function (key, value) {
    return store[key] || (store[key] = value !== undefined ? value : {});
  })('versions', []).push({
    version: _core.version,
    mode: _library ? 'pure' : 'global',
    copyright: '© 2018 Denis Pushkarev (zloirock.ru)'
  });
  });

  var _wks = createCommonjsModule(function (module) {
  var store = _shared('wks');

  var Symbol = _global.Symbol;
  var USE_SYMBOL = typeof Symbol == 'function';

  var $exports = module.exports = function (name) {
    return store[name] || (store[name] =
      USE_SYMBOL && Symbol[name] || (USE_SYMBOL ? Symbol : _uid)('Symbol.' + name));
  };

  $exports.store = store;
  });

  var _fixReWks = function (KEY, length, exec) {
    var SYMBOL = _wks(KEY);
    var fns = exec(_defined, SYMBOL, ''[KEY]);
    var strfn = fns[0];
    var rxfn = fns[1];
    if (_fails(function () {
      var O = {};
      O[SYMBOL] = function () { return 7; };
      return ''[KEY](O) != 7;
    })) {
      _redefine(String.prototype, KEY, strfn);
      _hide(RegExp.prototype, SYMBOL, length == 2
        // 21.2.5.8 RegExp.prototype[@@replace](string, replaceValue)
        // 21.2.5.11 RegExp.prototype[@@split](string, limit)
        ? function (string, arg) { return rxfn.call(string, this, arg); }
        // 21.2.5.6 RegExp.prototype[@@match](string)
        // 21.2.5.9 RegExp.prototype[@@search](string)
        : function (string) { return rxfn.call(string, this); }
      );
    }
  };

  // @@replace logic
  _fixReWks('replace', 2, function (defined, REPLACE, $replace) {
    // 21.1.3.14 String.prototype.replace(searchValue, replaceValue)
    return [function replace(searchValue, replaceValue) {
      var O = defined(this);
      var fn = searchValue == undefined ? undefined : searchValue[REPLACE];
      return fn !== undefined
        ? fn.call(searchValue, O, replaceValue)
        : $replace.call(String(O), searchValue, replaceValue);
    }, $replace];
  });

  var dP$1 = _objectDp.f;
  var FProto = Function.prototype;
  var nameRE = /^\s*function ([^ (]*)/;
  var NAME = 'name';

  // 19.2.4.2 name
  NAME in FProto || _descriptors && dP$1(FProto, NAME, {
    configurable: true,
    get: function () {
      try {
        return ('' + this).match(nameRE)[1];
      } catch (e) {
        return '';
      }
    }
  });

  // @@match logic
  _fixReWks('match', 1, function (defined, MATCH, $match) {
    // 21.1.3.11 String.prototype.match(regexp)
    return [function match(regexp) {
      var O = defined(this);
      var fn = regexp == undefined ? undefined : regexp[MATCH];
      return fn !== undefined ? fn.call(regexp, O) : new RegExp(regexp)[MATCH](String(O));
    }, $match];
  });

  // 22.1.3.31 Array.prototype[@@unscopables]
  var UNSCOPABLES = _wks('unscopables');
  var ArrayProto = Array.prototype;
  if (ArrayProto[UNSCOPABLES] == undefined) _hide(ArrayProto, UNSCOPABLES, {});
  var _addToUnscopables = function (key) {
    ArrayProto[UNSCOPABLES][key] = true;
  };

  var _iterStep = function (done, value) {
    return { value: value, done: !!done };
  };

  var _iterators = {};

  var toString = {}.toString;

  var _cof = function (it) {
    return toString.call(it).slice(8, -1);
  };

  // fallback for non-array-like ES3 and non-enumerable old V8 strings

  // eslint-disable-next-line no-prototype-builtins
  var _iobject = Object('z').propertyIsEnumerable(0) ? Object : function (it) {
    return _cof(it) == 'String' ? it.split('') : Object(it);
  };

  // to indexed object, toObject with fallback for non-array-like ES3 strings


  var _toIobject = function (it) {
    return _iobject(_defined(it));
  };

  var _aFunction = function (it) {
    if (typeof it != 'function') throw TypeError(it + ' is not a function!');
    return it;
  };

  // optional / simple context binding

  var _ctx = function (fn, that, length) {
    _aFunction(fn);
    if (that === undefined) return fn;
    switch (length) {
      case 1: return function (a) {
        return fn.call(that, a);
      };
      case 2: return function (a, b) {
        return fn.call(that, a, b);
      };
      case 3: return function (a, b, c) {
        return fn.call(that, a, b, c);
      };
    }
    return function (/* ...args */) {
      return fn.apply(that, arguments);
    };
  };

  var PROTOTYPE = 'prototype';

  var $export = function (type, name, source) {
    var IS_FORCED = type & $export.F;
    var IS_GLOBAL = type & $export.G;
    var IS_STATIC = type & $export.S;
    var IS_PROTO = type & $export.P;
    var IS_BIND = type & $export.B;
    var target = IS_GLOBAL ? _global : IS_STATIC ? _global[name] || (_global[name] = {}) : (_global[name] || {})[PROTOTYPE];
    var exports = IS_GLOBAL ? _core : _core[name] || (_core[name] = {});
    var expProto = exports[PROTOTYPE] || (exports[PROTOTYPE] = {});
    var key, own, out, exp;
    if (IS_GLOBAL) source = name;
    for (key in source) {
      // contains in native
      own = !IS_FORCED && target && target[key] !== undefined;
      // export native or passed
      out = (own ? target : source)[key];
      // bind timers to global for call from export context
      exp = IS_BIND && own ? _ctx(out, _global) : IS_PROTO && typeof out == 'function' ? _ctx(Function.call, out) : out;
      // extend global
      if (target) _redefine(target, key, out, type & $export.U);
      // export
      if (exports[key] != out) _hide(exports, key, exp);
      if (IS_PROTO && expProto[key] != out) expProto[key] = out;
    }
  };
  _global.core = _core;
  // type bitmap
  $export.F = 1;   // forced
  $export.G = 2;   // global
  $export.S = 4;   // static
  $export.P = 8;   // proto
  $export.B = 16;  // bind
  $export.W = 32;  // wrap
  $export.U = 64;  // safe
  $export.R = 128; // real proto method for `library`
  var _export = $export;

  // 7.1.4 ToInteger
  var ceil = Math.ceil;
  var floor = Math.floor;
  var _toInteger = function (it) {
    return isNaN(it = +it) ? 0 : (it > 0 ? floor : ceil)(it);
  };

  // 7.1.15 ToLength

  var min = Math.min;
  var _toLength = function (it) {
    return it > 0 ? min(_toInteger(it), 0x1fffffffffffff) : 0; // pow(2, 53) - 1 == 9007199254740991
  };

  var max = Math.max;
  var min$1 = Math.min;
  var _toAbsoluteIndex = function (index, length) {
    index = _toInteger(index);
    return index < 0 ? max(index + length, 0) : min$1(index, length);
  };

  // false -> Array#indexOf
  // true  -> Array#includes



  var _arrayIncludes = function (IS_INCLUDES) {
    return function ($this, el, fromIndex) {
      var O = _toIobject($this);
      var length = _toLength(O.length);
      var index = _toAbsoluteIndex(fromIndex, length);
      var value;
      // Array#includes uses SameValueZero equality algorithm
      // eslint-disable-next-line no-self-compare
      if (IS_INCLUDES && el != el) while (length > index) {
        value = O[index++];
        // eslint-disable-next-line no-self-compare
        if (value != value) return true;
      // Array#indexOf ignores holes, Array#includes - not
      } else for (;length > index; index++) if (IS_INCLUDES || index in O) {
        if (O[index] === el) return IS_INCLUDES || index || 0;
      } return !IS_INCLUDES && -1;
    };
  };

  var shared = _shared('keys');

  var _sharedKey = function (key) {
    return shared[key] || (shared[key] = _uid(key));
  };

  var arrayIndexOf = _arrayIncludes(false);
  var IE_PROTO = _sharedKey('IE_PROTO');

  var _objectKeysInternal = function (object, names) {
    var O = _toIobject(object);
    var i = 0;
    var result = [];
    var key;
    for (key in O) if (key != IE_PROTO) _has(O, key) && result.push(key);
    // Don't enum bug & hidden keys
    while (names.length > i) if (_has(O, key = names[i++])) {
      ~arrayIndexOf(result, key) || result.push(key);
    }
    return result;
  };

  // IE 8- don't enum bug keys
  var _enumBugKeys = (
    'constructor,hasOwnProperty,isPrototypeOf,propertyIsEnumerable,toLocaleString,toString,valueOf'
  ).split(',');

  // 19.1.2.14 / 15.2.3.14 Object.keys(O)



  var _objectKeys = Object.keys || function keys(O) {
    return _objectKeysInternal(O, _enumBugKeys);
  };

  var _objectDps = _descriptors ? Object.defineProperties : function defineProperties(O, Properties) {
    _anObject(O);
    var keys = _objectKeys(Properties);
    var length = keys.length;
    var i = 0;
    var P;
    while (length > i) _objectDp.f(O, P = keys[i++], Properties[P]);
    return O;
  };

  var document$2 = _global.document;
  var _html = document$2 && document$2.documentElement;

  // 19.1.2.2 / 15.2.3.5 Object.create(O [, Properties])



  var IE_PROTO$1 = _sharedKey('IE_PROTO');
  var Empty = function () { /* empty */ };
  var PROTOTYPE$1 = 'prototype';

  // Create object with fake `null` prototype: use iframe Object with cleared prototype
  var createDict = function () {
    // Thrash, waste and sodomy: IE GC bug
    var iframe = _domCreate('iframe');
    var i = _enumBugKeys.length;
    var lt = '<';
    var gt = '>';
    var iframeDocument;
    iframe.style.display = 'none';
    _html.appendChild(iframe);
    iframe.src = 'javascript:'; // eslint-disable-line no-script-url
    // createDict = iframe.contentWindow.Object;
    // html.removeChild(iframe);
    iframeDocument = iframe.contentWindow.document;
    iframeDocument.open();
    iframeDocument.write(lt + 'script' + gt + 'document.F=Object' + lt + '/script' + gt);
    iframeDocument.close();
    createDict = iframeDocument.F;
    while (i--) delete createDict[PROTOTYPE$1][_enumBugKeys[i]];
    return createDict();
  };

  var _objectCreate = Object.create || function create(O, Properties) {
    var result;
    if (O !== null) {
      Empty[PROTOTYPE$1] = _anObject(O);
      result = new Empty();
      Empty[PROTOTYPE$1] = null;
      // add "__proto__" for Object.getPrototypeOf polyfill
      result[IE_PROTO$1] = O;
    } else result = createDict();
    return Properties === undefined ? result : _objectDps(result, Properties);
  };

  var def = _objectDp.f;

  var TAG = _wks('toStringTag');

  var _setToStringTag = function (it, tag, stat) {
    if (it && !_has(it = stat ? it : it.prototype, TAG)) def(it, TAG, { configurable: true, value: tag });
  };

  var IteratorPrototype = {};

  // 25.1.2.1.1 %IteratorPrototype%[@@iterator]()
  _hide(IteratorPrototype, _wks('iterator'), function () { return this; });

  var _iterCreate = function (Constructor, NAME, next) {
    Constructor.prototype = _objectCreate(IteratorPrototype, { next: _propertyDesc(1, next) });
    _setToStringTag(Constructor, NAME + ' Iterator');
  };

  // 7.1.13 ToObject(argument)

  var _toObject = function (it) {
    return Object(_defined(it));
  };

  // 19.1.2.9 / 15.2.3.2 Object.getPrototypeOf(O)


  var IE_PROTO$2 = _sharedKey('IE_PROTO');
  var ObjectProto = Object.prototype;

  var _objectGpo = Object.getPrototypeOf || function (O) {
    O = _toObject(O);
    if (_has(O, IE_PROTO$2)) return O[IE_PROTO$2];
    if (typeof O.constructor == 'function' && O instanceof O.constructor) {
      return O.constructor.prototype;
    } return O instanceof Object ? ObjectProto : null;
  };

  var ITERATOR = _wks('iterator');
  var BUGGY = !([].keys && 'next' in [].keys()); // Safari has buggy iterators w/o `next`
  var FF_ITERATOR = '@@iterator';
  var KEYS = 'keys';
  var VALUES = 'values';

  var returnThis = function () { return this; };

  var _iterDefine = function (Base, NAME, Constructor, next, DEFAULT, IS_SET, FORCED) {
    _iterCreate(Constructor, NAME, next);
    var getMethod = function (kind) {
      if (!BUGGY && kind in proto) return proto[kind];
      switch (kind) {
        case KEYS: return function keys() { return new Constructor(this, kind); };
        case VALUES: return function values() { return new Constructor(this, kind); };
      } return function entries() { return new Constructor(this, kind); };
    };
    var TAG = NAME + ' Iterator';
    var DEF_VALUES = DEFAULT == VALUES;
    var VALUES_BUG = false;
    var proto = Base.prototype;
    var $native = proto[ITERATOR] || proto[FF_ITERATOR] || DEFAULT && proto[DEFAULT];
    var $default = $native || getMethod(DEFAULT);
    var $entries = DEFAULT ? !DEF_VALUES ? $default : getMethod('entries') : undefined;
    var $anyNative = NAME == 'Array' ? proto.entries || $native : $native;
    var methods, key, IteratorPrototype;
    // Fix native
    if ($anyNative) {
      IteratorPrototype = _objectGpo($anyNative.call(new Base()));
      if (IteratorPrototype !== Object.prototype && IteratorPrototype.next) {
        // Set @@toStringTag to native iterators
        _setToStringTag(IteratorPrototype, TAG, true);
        // fix for some old engines
        if (!_library && typeof IteratorPrototype[ITERATOR] != 'function') _hide(IteratorPrototype, ITERATOR, returnThis);
      }
    }
    // fix Array#{values, @@iterator}.name in V8 / FF
    if (DEF_VALUES && $native && $native.name !== VALUES) {
      VALUES_BUG = true;
      $default = function values() { return $native.call(this); };
    }
    // Define iterator
    if ((!_library || FORCED) && (BUGGY || VALUES_BUG || !proto[ITERATOR])) {
      _hide(proto, ITERATOR, $default);
    }
    // Plug for library
    _iterators[NAME] = $default;
    _iterators[TAG] = returnThis;
    if (DEFAULT) {
      methods = {
        values: DEF_VALUES ? $default : getMethod(VALUES),
        keys: IS_SET ? $default : getMethod(KEYS),
        entries: $entries
      };
      if (FORCED) for (key in methods) {
        if (!(key in proto)) _redefine(proto, key, methods[key]);
      } else _export(_export.P + _export.F * (BUGGY || VALUES_BUG), NAME, methods);
    }
    return methods;
  };

  // 22.1.3.4 Array.prototype.entries()
  // 22.1.3.13 Array.prototype.keys()
  // 22.1.3.29 Array.prototype.values()
  // 22.1.3.30 Array.prototype[@@iterator]()
  var es6_array_iterator = _iterDefine(Array, 'Array', function (iterated, kind) {
    this._t = _toIobject(iterated); // target
    this._i = 0;                   // next index
    this._k = kind;                // kind
  // 22.1.5.2.1 %ArrayIteratorPrototype%.next()
  }, function () {
    var O = this._t;
    var kind = this._k;
    var index = this._i++;
    if (!O || index >= O.length) {
      this._t = undefined;
      return _iterStep(1);
    }
    if (kind == 'keys') return _iterStep(0, index);
    if (kind == 'values') return _iterStep(0, O[index]);
    return _iterStep(0, [index, O[index]]);
  }, 'values');

  // argumentsList[@@iterator] is %ArrayProto_values% (9.4.4.6, 9.4.4.7)
  _iterators.Arguments = _iterators.Array;

  _addToUnscopables('keys');
  _addToUnscopables('values');
  _addToUnscopables('entries');

  var ITERATOR$1 = _wks('iterator');
  var TO_STRING_TAG = _wks('toStringTag');
  var ArrayValues = _iterators.Array;

  var DOMIterables = {
    CSSRuleList: true, // TODO: Not spec compliant, should be false.
    CSSStyleDeclaration: false,
    CSSValueList: false,
    ClientRectList: false,
    DOMRectList: false,
    DOMStringList: false,
    DOMTokenList: true,
    DataTransferItemList: false,
    FileList: false,
    HTMLAllCollection: false,
    HTMLCollection: false,
    HTMLFormElement: false,
    HTMLSelectElement: false,
    MediaList: true, // TODO: Not spec compliant, should be false.
    MimeTypeArray: false,
    NamedNodeMap: false,
    NodeList: true,
    PaintRequestList: false,
    Plugin: false,
    PluginArray: false,
    SVGLengthList: false,
    SVGNumberList: false,
    SVGPathSegList: false,
    SVGPointList: false,
    SVGStringList: false,
    SVGTransformList: false,
    SourceBufferList: false,
    StyleSheetList: true, // TODO: Not spec compliant, should be false.
    TextTrackCueList: false,
    TextTrackList: false,
    TouchList: false
  };

  for (var collections = _objectKeys(DOMIterables), i = 0; i < collections.length; i++) {
    var NAME$1 = collections[i];
    var explicit = DOMIterables[NAME$1];
    var Collection = _global[NAME$1];
    var proto = Collection && Collection.prototype;
    var key;
    if (proto) {
      if (!proto[ITERATOR$1]) _hide(proto, ITERATOR$1, ArrayValues);
      if (!proto[TO_STRING_TAG]) _hide(proto, TO_STRING_TAG, NAME$1);
      _iterators[NAME$1] = ArrayValues;
      if (explicit) for (key in es6_array_iterator) if (!proto[key]) _redefine(proto, key, es6_array_iterator[key], true);
    }
  }

  // true  -> String#at
  // false -> String#codePointAt
  var _stringAt = function (TO_STRING) {
    return function (that, pos) {
      var s = String(_defined(that));
      var i = _toInteger(pos);
      var l = s.length;
      var a, b;
      if (i < 0 || i >= l) return TO_STRING ? '' : undefined;
      a = s.charCodeAt(i);
      return a < 0xd800 || a > 0xdbff || i + 1 === l || (b = s.charCodeAt(i + 1)) < 0xdc00 || b > 0xdfff
        ? TO_STRING ? s.charAt(i) : a
        : TO_STRING ? s.slice(i, i + 2) : (a - 0xd800 << 10) + (b - 0xdc00) + 0x10000;
    };
  };

  var $at = _stringAt(true);

  // 21.1.3.27 String.prototype[@@iterator]()
  _iterDefine(String, 'String', function (iterated) {
    this._t = String(iterated); // target
    this._i = 0;                // next index
  // 21.1.5.2.1 %StringIteratorPrototype%.next()
  }, function () {
    var O = this._t;
    var index = this._i;
    var point;
    if (index >= O.length) return { value: undefined, done: true };
    point = $at(O, index);
    this._i += point.length;
    return { value: point, done: false };
  });

  // call something on iterator step with safe closing on error

  var _iterCall = function (iterator, fn, value, entries) {
    try {
      return entries ? fn(_anObject(value)[0], value[1]) : fn(value);
    // 7.4.6 IteratorClose(iterator, completion)
    } catch (e) {
      var ret = iterator['return'];
      if (ret !== undefined) _anObject(ret.call(iterator));
      throw e;
    }
  };

  // check on default Array iterator

  var ITERATOR$2 = _wks('iterator');
  var ArrayProto$1 = Array.prototype;

  var _isArrayIter = function (it) {
    return it !== undefined && (_iterators.Array === it || ArrayProto$1[ITERATOR$2] === it);
  };

  var _createProperty = function (object, index, value) {
    if (index in object) _objectDp.f(object, index, _propertyDesc(0, value));
    else object[index] = value;
  };

  // getting tag from 19.1.3.6 Object.prototype.toString()

  var TAG$1 = _wks('toStringTag');
  // ES3 wrong here
  var ARG = _cof(function () { return arguments; }()) == 'Arguments';

  // fallback for IE11 Script Access Denied error
  var tryGet = function (it, key) {
    try {
      return it[key];
    } catch (e) { /* empty */ }
  };

  var _classof = function (it) {
    var O, T, B;
    return it === undefined ? 'Undefined' : it === null ? 'Null'
      // @@toStringTag case
      : typeof (T = tryGet(O = Object(it), TAG$1)) == 'string' ? T
      // builtinTag case
      : ARG ? _cof(O)
      // ES3 arguments fallback
      : (B = _cof(O)) == 'Object' && typeof O.callee == 'function' ? 'Arguments' : B;
  };

  var ITERATOR$3 = _wks('iterator');

  var core_getIteratorMethod = _core.getIteratorMethod = function (it) {
    if (it != undefined) return it[ITERATOR$3]
      || it['@@iterator']
      || _iterators[_classof(it)];
  };

  var ITERATOR$4 = _wks('iterator');
  var SAFE_CLOSING = false;

  try {
    var riter = [7][ITERATOR$4]();
    riter['return'] = function () { SAFE_CLOSING = true; };
  } catch (e) { /* empty */ }

  var _iterDetect = function (exec, skipClosing) {
    if (!skipClosing && !SAFE_CLOSING) return false;
    var safe = false;
    try {
      var arr = [7];
      var iter = arr[ITERATOR$4]();
      iter.next = function () { return { done: safe = true }; };
      arr[ITERATOR$4] = function () { return iter; };
      exec(arr);
    } catch (e) { /* empty */ }
    return safe;
  };

  _export(_export.S + _export.F * !_iterDetect(function (iter) { }), 'Array', {
    // 22.1.2.1 Array.from(arrayLike, mapfn = undefined, thisArg = undefined)
    from: function from(arrayLike /* , mapfn = undefined, thisArg = undefined */) {
      var O = _toObject(arrayLike);
      var C = typeof this == 'function' ? this : Array;
      var aLen = arguments.length;
      var mapfn = aLen > 1 ? arguments[1] : undefined;
      var mapping = mapfn !== undefined;
      var index = 0;
      var iterFn = core_getIteratorMethod(O);
      var length, result, step, iterator;
      if (mapping) mapfn = _ctx(mapfn, aLen > 2 ? arguments[2] : undefined, 2);
      // if object isn't iterable or it's array with default iterator - use simple case
      if (iterFn != undefined && !(C == Array && _isArrayIter(iterFn))) {
        for (iterator = iterFn.call(O), result = new C(); !(step = iterator.next()).done; index++) {
          _createProperty(result, index, mapping ? _iterCall(iterator, mapfn, [step.value, index], true) : step.value);
        }
      } else {
        length = _toLength(O.length);
        for (result = new C(length); length > index; index++) {
          _createProperty(result, index, mapping ? mapfn(O[index], index) : O[index]);
        }
      }
      result.length = index;
      return result;
    }
  });

  var f$1 = Object.getOwnPropertySymbols;

  var _objectGops = {
    f: f$1
  };

  var f$2 = {}.propertyIsEnumerable;

  var _objectPie = {
    f: f$2
  };

  // 19.1.2.1 Object.assign(target, source, ...)





  var $assign = Object.assign;

  // should work with symbols and should have deterministic property order (V8 bug)
  var _objectAssign = !$assign || _fails(function () {
    var A = {};
    var B = {};
    // eslint-disable-next-line no-undef
    var S = Symbol();
    var K = 'abcdefghijklmnopqrst';
    A[S] = 7;
    K.split('').forEach(function (k) { B[k] = k; });
    return $assign({}, A)[S] != 7 || Object.keys($assign({}, B)).join('') != K;
  }) ? function assign(target, source) { // eslint-disable-line no-unused-vars
    var T = _toObject(target);
    var aLen = arguments.length;
    var index = 1;
    var getSymbols = _objectGops.f;
    var isEnum = _objectPie.f;
    while (aLen > index) {
      var S = _iobject(arguments[index++]);
      var keys = getSymbols ? _objectKeys(S).concat(getSymbols(S)) : _objectKeys(S);
      var length = keys.length;
      var j = 0;
      var key;
      while (length > j) if (isEnum.call(S, key = keys[j++])) T[key] = S[key];
    } return T;
  } : $assign;

  // 19.1.3.1 Object.assign(target, source)


  _export(_export.S + _export.F, 'Object', { assign: _objectAssign });

  function _classCallCheck(instance, Constructor) {
    if (!(instance instanceof Constructor)) {
      throw new TypeError("Cannot call a class as a function");
    }
  }

  function _defineProperties(target, props) {
    for (var i = 0; i < props.length; i++) {
      var descriptor = props[i];
      descriptor.enumerable = descriptor.enumerable || false;
      descriptor.configurable = true;
      if ("value" in descriptor) descriptor.writable = true;
      Object.defineProperty(target, descriptor.key, descriptor);
    }
  }

  function _createClass(Constructor, protoProps, staticProps) {
    if (protoProps) _defineProperties(Constructor.prototype, protoProps);
    if (staticProps) _defineProperties(Constructor, staticProps);
    return Constructor;
  }

  var scrollbarWidth = createCommonjsModule(function (module, exports) {
  /*! scrollbarWidth.js v0.1.3 | felixexter | MIT | https://github.com/felixexter/scrollbarWidth */
  (function (root, factory) {
    {
      module.exports = factory();
    }
  }(commonjsGlobal, function () {

    function scrollbarWidth() {
      if (typeof document === 'undefined') {
        return 0
      }

      var
        body = document.body,
        box = document.createElement('div'),
        boxStyle = box.style,
        width;

      boxStyle.position = 'absolute';
      boxStyle.top = boxStyle.left = '-9999px';
      boxStyle.width = boxStyle.height = '100px';
      boxStyle.overflow = 'scroll';

      body.appendChild(box);

      width = box.offsetWidth - box.clientWidth;

      body.removeChild(box);

      return width;
    }

    return scrollbarWidth;
  }));
  });

  /**
   * lodash (Custom Build) <https://lodash.com/>
   * Build: `lodash modularize exports="npm" -o ./`
   * Copyright jQuery Foundation and other contributors <https://jquery.org/>
   * Released under MIT license <https://lodash.com/license>
   * Based on Underscore.js 1.8.3 <http://underscorejs.org/LICENSE>
   * Copyright Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
   */

  /** Used as the `TypeError` message for "Functions" methods. */
  var FUNC_ERROR_TEXT = 'Expected a function';

  /** Used as references for various `Number` constants. */
  var NAN = 0 / 0;

  /** `Object#toString` result references. */
  var symbolTag = '[object Symbol]';

  /** Used to match leading and trailing whitespace. */
  var reTrim = /^\s+|\s+$/g;

  /** Used to detect bad signed hexadecimal string values. */
  var reIsBadHex = /^[-+]0x[0-9a-f]+$/i;

  /** Used to detect binary string values. */
  var reIsBinary = /^0b[01]+$/i;

  /** Used to detect octal string values. */
  var reIsOctal = /^0o[0-7]+$/i;

  /** Built-in method references without a dependency on `root`. */
  var freeParseInt = parseInt;

  /** Detect free variable `global` from Node.js. */
  var freeGlobal = typeof commonjsGlobal == 'object' && commonjsGlobal && commonjsGlobal.Object === Object && commonjsGlobal;

  /** Detect free variable `self`. */
  var freeSelf = typeof self == 'object' && self && self.Object === Object && self;

  /** Used as a reference to the global object. */
  var root = freeGlobal || freeSelf || Function('return this')();

  /** Used for built-in method references. */
  var objectProto = Object.prototype;

  /**
   * Used to resolve the
   * [`toStringTag`](http://ecma-international.org/ecma-262/7.0/#sec-object.prototype.tostring)
   * of values.
   */
  var objectToString = objectProto.toString;

  /* Built-in method references for those with the same name as other `lodash` methods. */
  var nativeMax = Math.max,
      nativeMin = Math.min;

  /**
   * Gets the timestamp of the number of milliseconds that have elapsed since
   * the Unix epoch (1 January 1970 00:00:00 UTC).
   *
   * @static
   * @memberOf _
   * @since 2.4.0
   * @category Date
   * @returns {number} Returns the timestamp.
   * @example
   *
   * _.defer(function(stamp) {
   *   console.log(_.now() - stamp);
   * }, _.now());
   * // => Logs the number of milliseconds it took for the deferred invocation.
   */
  var now = function() {
    return root.Date.now();
  };

  /**
   * Creates a debounced function that delays invoking `func` until after `wait`
   * milliseconds have elapsed since the last time the debounced function was
   * invoked. The debounced function comes with a `cancel` method to cancel
   * delayed `func` invocations and a `flush` method to immediately invoke them.
   * Provide `options` to indicate whether `func` should be invoked on the
   * leading and/or trailing edge of the `wait` timeout. The `func` is invoked
   * with the last arguments provided to the debounced function. Subsequent
   * calls to the debounced function return the result of the last `func`
   * invocation.
   *
   * **Note:** If `leading` and `trailing` options are `true`, `func` is
   * invoked on the trailing edge of the timeout only if the debounced function
   * is invoked more than once during the `wait` timeout.
   *
   * If `wait` is `0` and `leading` is `false`, `func` invocation is deferred
   * until to the next tick, similar to `setTimeout` with a timeout of `0`.
   *
   * See [David Corbacho's article](https://css-tricks.com/debouncing-throttling-explained-examples/)
   * for details over the differences between `_.debounce` and `_.throttle`.
   *
   * @static
   * @memberOf _
   * @since 0.1.0
   * @category Function
   * @param {Function} func The function to debounce.
   * @param {number} [wait=0] The number of milliseconds to delay.
   * @param {Object} [options={}] The options object.
   * @param {boolean} [options.leading=false]
   *  Specify invoking on the leading edge of the timeout.
   * @param {number} [options.maxWait]
   *  The maximum time `func` is allowed to be delayed before it's invoked.
   * @param {boolean} [options.trailing=true]
   *  Specify invoking on the trailing edge of the timeout.
   * @returns {Function} Returns the new debounced function.
   * @example
   *
   * // Avoid costly calculations while the window size is in flux.
   * jQuery(window).on('resize', _.debounce(calculateLayout, 150));
   *
   * // Invoke `sendMail` when clicked, debouncing subsequent calls.
   * jQuery(element).on('click', _.debounce(sendMail, 300, {
   *   'leading': true,
   *   'trailing': false
   * }));
   *
   * // Ensure `batchLog` is invoked once after 1 second of debounced calls.
   * var debounced = _.debounce(batchLog, 250, { 'maxWait': 1000 });
   * var source = new EventSource('/stream');
   * jQuery(source).on('message', debounced);
   *
   * // Cancel the trailing debounced invocation.
   * jQuery(window).on('popstate', debounced.cancel);
   */
  function debounce(func, wait, options) {
    var lastArgs,
        lastThis,
        maxWait,
        result,
        timerId,
        lastCallTime,
        lastInvokeTime = 0,
        leading = false,
        maxing = false,
        trailing = true;

    if (typeof func != 'function') {
      throw new TypeError(FUNC_ERROR_TEXT);
    }
    wait = toNumber(wait) || 0;
    if (isObject(options)) {
      leading = !!options.leading;
      maxing = 'maxWait' in options;
      maxWait = maxing ? nativeMax(toNumber(options.maxWait) || 0, wait) : maxWait;
      trailing = 'trailing' in options ? !!options.trailing : trailing;
    }

    function invokeFunc(time) {
      var args = lastArgs,
          thisArg = lastThis;

      lastArgs = lastThis = undefined;
      lastInvokeTime = time;
      result = func.apply(thisArg, args);
      return result;
    }

    function leadingEdge(time) {
      // Reset any `maxWait` timer.
      lastInvokeTime = time;
      // Start the timer for the trailing edge.
      timerId = setTimeout(timerExpired, wait);
      // Invoke the leading edge.
      return leading ? invokeFunc(time) : result;
    }

    function remainingWait(time) {
      var timeSinceLastCall = time - lastCallTime,
          timeSinceLastInvoke = time - lastInvokeTime,
          result = wait - timeSinceLastCall;

      return maxing ? nativeMin(result, maxWait - timeSinceLastInvoke) : result;
    }

    function shouldInvoke(time) {
      var timeSinceLastCall = time - lastCallTime,
          timeSinceLastInvoke = time - lastInvokeTime;

      // Either this is the first call, activity has stopped and we're at the
      // trailing edge, the system time has gone backwards and we're treating
      // it as the trailing edge, or we've hit the `maxWait` limit.
      return (lastCallTime === undefined || (timeSinceLastCall >= wait) ||
        (timeSinceLastCall < 0) || (maxing && timeSinceLastInvoke >= maxWait));
    }

    function timerExpired() {
      var time = now();
      if (shouldInvoke(time)) {
        return trailingEdge(time);
      }
      // Restart the timer.
      timerId = setTimeout(timerExpired, remainingWait(time));
    }

    function trailingEdge(time) {
      timerId = undefined;

      // Only invoke if we have `lastArgs` which means `func` has been
      // debounced at least once.
      if (trailing && lastArgs) {
        return invokeFunc(time);
      }
      lastArgs = lastThis = undefined;
      return result;
    }

    function cancel() {
      if (timerId !== undefined) {
        clearTimeout(timerId);
      }
      lastInvokeTime = 0;
      lastArgs = lastCallTime = lastThis = timerId = undefined;
    }

    function flush() {
      return timerId === undefined ? result : trailingEdge(now());
    }

    function debounced() {
      var time = now(),
          isInvoking = shouldInvoke(time);

      lastArgs = arguments;
      lastThis = this;
      lastCallTime = time;

      if (isInvoking) {
        if (timerId === undefined) {
          return leadingEdge(lastCallTime);
        }
        if (maxing) {
          // Handle invocations in a tight loop.
          timerId = setTimeout(timerExpired, wait);
          return invokeFunc(lastCallTime);
        }
      }
      if (timerId === undefined) {
        timerId = setTimeout(timerExpired, wait);
      }
      return result;
    }
    debounced.cancel = cancel;
    debounced.flush = flush;
    return debounced;
  }

  /**
   * Creates a throttled function that only invokes `func` at most once per
   * every `wait` milliseconds. The throttled function comes with a `cancel`
   * method to cancel delayed `func` invocations and a `flush` method to
   * immediately invoke them. Provide `options` to indicate whether `func`
   * should be invoked on the leading and/or trailing edge of the `wait`
   * timeout. The `func` is invoked with the last arguments provided to the
   * throttled function. Subsequent calls to the throttled function return the
   * result of the last `func` invocation.
   *
   * **Note:** If `leading` and `trailing` options are `true`, `func` is
   * invoked on the trailing edge of the timeout only if the throttled function
   * is invoked more than once during the `wait` timeout.
   *
   * If `wait` is `0` and `leading` is `false`, `func` invocation is deferred
   * until to the next tick, similar to `setTimeout` with a timeout of `0`.
   *
   * See [David Corbacho's article](https://css-tricks.com/debouncing-throttling-explained-examples/)
   * for details over the differences between `_.throttle` and `_.debounce`.
   *
   * @static
   * @memberOf _
   * @since 0.1.0
   * @category Function
   * @param {Function} func The function to throttle.
   * @param {number} [wait=0] The number of milliseconds to throttle invocations to.
   * @param {Object} [options={}] The options object.
   * @param {boolean} [options.leading=true]
   *  Specify invoking on the leading edge of the timeout.
   * @param {boolean} [options.trailing=true]
   *  Specify invoking on the trailing edge of the timeout.
   * @returns {Function} Returns the new throttled function.
   * @example
   *
   * // Avoid excessively updating the position while scrolling.
   * jQuery(window).on('scroll', _.throttle(updatePosition, 100));
   *
   * // Invoke `renewToken` when the click event is fired, but not more than once every 5 minutes.
   * var throttled = _.throttle(renewToken, 300000, { 'trailing': false });
   * jQuery(element).on('click', throttled);
   *
   * // Cancel the trailing throttled invocation.
   * jQuery(window).on('popstate', throttled.cancel);
   */
  function throttle(func, wait, options) {
    var leading = true,
        trailing = true;

    if (typeof func != 'function') {
      throw new TypeError(FUNC_ERROR_TEXT);
    }
    if (isObject(options)) {
      leading = 'leading' in options ? !!options.leading : leading;
      trailing = 'trailing' in options ? !!options.trailing : trailing;
    }
    return debounce(func, wait, {
      'leading': leading,
      'maxWait': wait,
      'trailing': trailing
    });
  }

  /**
   * Checks if `value` is the
   * [language type](http://www.ecma-international.org/ecma-262/7.0/#sec-ecmascript-language-types)
   * of `Object`. (e.g. arrays, functions, objects, regexes, `new Number(0)`, and `new String('')`)
   *
   * @static
   * @memberOf _
   * @since 0.1.0
   * @category Lang
   * @param {*} value The value to check.
   * @returns {boolean} Returns `true` if `value` is an object, else `false`.
   * @example
   *
   * _.isObject({});
   * // => true
   *
   * _.isObject([1, 2, 3]);
   * // => true
   *
   * _.isObject(_.noop);
   * // => true
   *
   * _.isObject(null);
   * // => false
   */
  function isObject(value) {
    var type = typeof value;
    return !!value && (type == 'object' || type == 'function');
  }

  /**
   * Checks if `value` is object-like. A value is object-like if it's not `null`
   * and has a `typeof` result of "object".
   *
   * @static
   * @memberOf _
   * @since 4.0.0
   * @category Lang
   * @param {*} value The value to check.
   * @returns {boolean} Returns `true` if `value` is object-like, else `false`.
   * @example
   *
   * _.isObjectLike({});
   * // => true
   *
   * _.isObjectLike([1, 2, 3]);
   * // => true
   *
   * _.isObjectLike(_.noop);
   * // => false
   *
   * _.isObjectLike(null);
   * // => false
   */
  function isObjectLike(value) {
    return !!value && typeof value == 'object';
  }

  /**
   * Checks if `value` is classified as a `Symbol` primitive or object.
   *
   * @static
   * @memberOf _
   * @since 4.0.0
   * @category Lang
   * @param {*} value The value to check.
   * @returns {boolean} Returns `true` if `value` is a symbol, else `false`.
   * @example
   *
   * _.isSymbol(Symbol.iterator);
   * // => true
   *
   * _.isSymbol('abc');
   * // => false
   */
  function isSymbol(value) {
    return typeof value == 'symbol' ||
      (isObjectLike(value) && objectToString.call(value) == symbolTag);
  }

  /**
   * Converts `value` to a number.
   *
   * @static
   * @memberOf _
   * @since 4.0.0
   * @category Lang
   * @param {*} value The value to process.
   * @returns {number} Returns the number.
   * @example
   *
   * _.toNumber(3.2);
   * // => 3.2
   *
   * _.toNumber(Number.MIN_VALUE);
   * // => 5e-324
   *
   * _.toNumber(Infinity);
   * // => Infinity
   *
   * _.toNumber('3.2');
   * // => 3.2
   */
  function toNumber(value) {
    if (typeof value == 'number') {
      return value;
    }
    if (isSymbol(value)) {
      return NAN;
    }
    if (isObject(value)) {
      var other = typeof value.valueOf == 'function' ? value.valueOf() : value;
      value = isObject(other) ? (other + '') : other;
    }
    if (typeof value != 'string') {
      return value === 0 ? value : +value;
    }
    value = value.replace(reTrim, '');
    var isBinary = reIsBinary.test(value);
    return (isBinary || reIsOctal.test(value))
      ? freeParseInt(value.slice(2), isBinary ? 2 : 8)
      : (reIsBadHex.test(value) ? NAN : +value);
  }

  var lodash_throttle = throttle;

  /**
   * A collection of shims that provide minimal functionality of the ES6 collections.
   *
   * These implementations are not meant to be used outside of the ResizeObserver
   * modules as they cover only a limited range of use cases.
   */
  /* eslint-disable require-jsdoc, valid-jsdoc */
  var MapShim = (function () {
      if (typeof Map !== 'undefined') {
          return Map;
      }

      /**
       * Returns index in provided array that matches the specified key.
       *
       * @param {Array<Array>} arr
       * @param {*} key
       * @returns {number}
       */
      function getIndex(arr, key) {
          var result = -1;

          arr.some(function (entry, index) {
              if (entry[0] === key) {
                  result = index;

                  return true;
              }

              return false;
          });

          return result;
      }

      return (function () {
          function anonymous() {
              this.__entries__ = [];
          }

          var prototypeAccessors = { size: { configurable: true } };

          /**
           * @returns {boolean}
           */
          prototypeAccessors.size.get = function () {
              return this.__entries__.length;
          };

          /**
           * @param {*} key
           * @returns {*}
           */
          anonymous.prototype.get = function (key) {
              var index = getIndex(this.__entries__, key);
              var entry = this.__entries__[index];

              return entry && entry[1];
          };

          /**
           * @param {*} key
           * @param {*} value
           * @returns {void}
           */
          anonymous.prototype.set = function (key, value) {
              var index = getIndex(this.__entries__, key);

              if (~index) {
                  this.__entries__[index][1] = value;
              } else {
                  this.__entries__.push([key, value]);
              }
          };

          /**
           * @param {*} key
           * @returns {void}
           */
          anonymous.prototype.delete = function (key) {
              var entries = this.__entries__;
              var index = getIndex(entries, key);

              if (~index) {
                  entries.splice(index, 1);
              }
          };

          /**
           * @param {*} key
           * @returns {void}
           */
          anonymous.prototype.has = function (key) {
              return !!~getIndex(this.__entries__, key);
          };

          /**
           * @returns {void}
           */
          anonymous.prototype.clear = function () {
              this.__entries__.splice(0);
          };

          /**
           * @param {Function} callback
           * @param {*} [ctx=null]
           * @returns {void}
           */
          anonymous.prototype.forEach = function (callback, ctx) {
              var this$1 = this;
              if ( ctx === void 0 ) ctx = null;

              for (var i = 0, list = this$1.__entries__; i < list.length; i += 1) {
                  var entry = list[i];

                  callback.call(ctx, entry[1], entry[0]);
              }
          };

          Object.defineProperties( anonymous.prototype, prototypeAccessors );

          return anonymous;
      }());
  })();

  /**
   * Detects whether window and document objects are available in current environment.
   */
  var isBrowser = typeof window !== 'undefined' && typeof document !== 'undefined' && window.document === document;

  // Returns global object of a current environment.
  var global$1 = (function () {
      if (typeof global !== 'undefined' && global.Math === Math) {
          return global;
      }

      if (typeof self !== 'undefined' && self.Math === Math) {
          return self;
      }

      if (typeof window !== 'undefined' && window.Math === Math) {
          return window;
      }

      // eslint-disable-next-line no-new-func
      return Function('return this')();
  })();

  /**
   * A shim for the requestAnimationFrame which falls back to the setTimeout if
   * first one is not supported.
   *
   * @returns {number} Requests' identifier.
   */
  var requestAnimationFrame$1 = (function () {
      if (typeof requestAnimationFrame === 'function') {
          // It's required to use a bounded function because IE sometimes throws
          // an "Invalid calling object" error if rAF is invoked without the global
          // object on the left hand side.
          return requestAnimationFrame.bind(global$1);
      }

      return function (callback) { return setTimeout(function () { return callback(Date.now()); }, 1000 / 60); };
  })();

  // Defines minimum timeout before adding a trailing call.
  var trailingTimeout = 2;

  /**
   * Creates a wrapper function which ensures that provided callback will be
   * invoked only once during the specified delay period.
   *
   * @param {Function} callback - Function to be invoked after the delay period.
   * @param {number} delay - Delay after which to invoke callback.
   * @returns {Function}
   */
  var throttle$1 = function (callback, delay) {
      var leadingCall = false,
          trailingCall = false,
          lastCallTime = 0;

      /**
       * Invokes the original callback function and schedules new invocation if
       * the "proxy" was called during current request.
       *
       * @returns {void}
       */
      function resolvePending() {
          if (leadingCall) {
              leadingCall = false;

              callback();
          }

          if (trailingCall) {
              proxy();
          }
      }

      /**
       * Callback invoked after the specified delay. It will further postpone
       * invocation of the original function delegating it to the
       * requestAnimationFrame.
       *
       * @returns {void}
       */
      function timeoutCallback() {
          requestAnimationFrame$1(resolvePending);
      }

      /**
       * Schedules invocation of the original function.
       *
       * @returns {void}
       */
      function proxy() {
          var timeStamp = Date.now();

          if (leadingCall) {
              // Reject immediately following calls.
              if (timeStamp - lastCallTime < trailingTimeout) {
                  return;
              }

              // Schedule new call to be in invoked when the pending one is resolved.
              // This is important for "transitions" which never actually start
              // immediately so there is a chance that we might miss one if change
              // happens amids the pending invocation.
              trailingCall = true;
          } else {
              leadingCall = true;
              trailingCall = false;

              setTimeout(timeoutCallback, delay);
          }

          lastCallTime = timeStamp;
      }

      return proxy;
  };

  // Minimum delay before invoking the update of observers.
  var REFRESH_DELAY = 20;

  // A list of substrings of CSS properties used to find transition events that
  // might affect dimensions of observed elements.
  var transitionKeys = ['top', 'right', 'bottom', 'left', 'width', 'height', 'size', 'weight'];

  // Check if MutationObserver is available.
  var mutationObserverSupported = typeof MutationObserver !== 'undefined';

  /**
   * Singleton controller class which handles updates of ResizeObserver instances.
   */
  var ResizeObserverController = function() {
      this.connected_ = false;
      this.mutationEventsAdded_ = false;
      this.mutationsObserver_ = null;
      this.observers_ = [];

      this.onTransitionEnd_ = this.onTransitionEnd_.bind(this);
      this.refresh = throttle$1(this.refresh.bind(this), REFRESH_DELAY);
  };

  /**
   * Adds observer to observers list.
   *
   * @param {ResizeObserverSPI} observer - Observer to be added.
   * @returns {void}
   */


  /**
   * Holds reference to the controller's instance.
   *
   * @private {ResizeObserverController}
   */


  /**
   * Keeps reference to the instance of MutationObserver.
   *
   * @private {MutationObserver}
   */

  /**
   * Indicates whether DOM listeners have been added.
   *
   * @private {boolean}
   */
  ResizeObserverController.prototype.addObserver = function (observer) {
      if (!~this.observers_.indexOf(observer)) {
          this.observers_.push(observer);
      }

      // Add listeners if they haven't been added yet.
      if (!this.connected_) {
          this.connect_();
      }
  };

  /**
   * Removes observer from observers list.
   *
   * @param {ResizeObserverSPI} observer - Observer to be removed.
   * @returns {void}
   */
  ResizeObserverController.prototype.removeObserver = function (observer) {
      var observers = this.observers_;
      var index = observers.indexOf(observer);

      // Remove observer if it's present in registry.
      if (~index) {
          observers.splice(index, 1);
      }

      // Remove listeners if controller has no connected observers.
      if (!observers.length && this.connected_) {
          this.disconnect_();
      }
  };

  /**
   * Invokes the update of observers. It will continue running updates insofar
   * it detects changes.
   *
   * @returns {void}
   */
  ResizeObserverController.prototype.refresh = function () {
      var changesDetected = this.updateObservers_();

      // Continue running updates if changes have been detected as there might
      // be future ones caused by CSS transitions.
      if (changesDetected) {
          this.refresh();
      }
  };

  /**
   * Updates every observer from observers list and notifies them of queued
   * entries.
   *
   * @private
   * @returns {boolean} Returns "true" if any observer has detected changes in
   *  dimensions of it's elements.
   */
  ResizeObserverController.prototype.updateObservers_ = function () {
      // Collect observers that have active observations.
      var activeObservers = this.observers_.filter(function (observer) {
          return observer.gatherActive(), observer.hasActive();
      });

      // Deliver notifications in a separate cycle in order to avoid any
      // collisions between observers, e.g. when multiple instances of
      // ResizeObserver are tracking the same element and the callback of one
      // of them changes content dimensions of the observed target. Sometimes
      // this may result in notifications being blocked for the rest of observers.
      activeObservers.forEach(function (observer) { return observer.broadcastActive(); });

      return activeObservers.length > 0;
  };

  /**
   * Initializes DOM listeners.
   *
   * @private
   * @returns {void}
   */
  ResizeObserverController.prototype.connect_ = function () {
      // Do nothing if running in a non-browser environment or if listeners
      // have been already added.
      if (!isBrowser || this.connected_) {
          return;
      }

      // Subscription to the "Transitionend" event is used as a workaround for
      // delayed transitions. This way it's possible to capture at least the
      // final state of an element.
      document.addEventListener('transitionend', this.onTransitionEnd_);

      window.addEventListener('resize', this.refresh);

      if (mutationObserverSupported) {
          this.mutationsObserver_ = new MutationObserver(this.refresh);

          this.mutationsObserver_.observe(document, {
              attributes: true,
              childList: true,
              characterData: true,
              subtree: true
          });
      } else {
          document.addEventListener('DOMSubtreeModified', this.refresh);

          this.mutationEventsAdded_ = true;
      }

      this.connected_ = true;
  };

  /**
   * Removes DOM listeners.
   *
   * @private
   * @returns {void}
   */
  ResizeObserverController.prototype.disconnect_ = function () {
      // Do nothing if running in a non-browser environment or if listeners
      // have been already removed.
      if (!isBrowser || !this.connected_) {
          return;
      }

      document.removeEventListener('transitionend', this.onTransitionEnd_);
      window.removeEventListener('resize', this.refresh);

      if (this.mutationsObserver_) {
          this.mutationsObserver_.disconnect();
      }

      if (this.mutationEventsAdded_) {
          document.removeEventListener('DOMSubtreeModified', this.refresh);
      }

      this.mutationsObserver_ = null;
      this.mutationEventsAdded_ = false;
      this.connected_ = false;
  };

  /**
   * "Transitionend" event handler.
   *
   * @private
   * @param {TransitionEvent} event
   * @returns {void}
   */
  ResizeObserverController.prototype.onTransitionEnd_ = function (ref) {
          var propertyName = ref.propertyName; if ( propertyName === void 0 ) propertyName = '';

      // Detect whether transition may affect dimensions of an element.
      var isReflowProperty = transitionKeys.some(function (key) {
          return !!~propertyName.indexOf(key);
      });

      if (isReflowProperty) {
          this.refresh();
      }
  };

  /**
   * Returns instance of the ResizeObserverController.
   *
   * @returns {ResizeObserverController}
   */
  ResizeObserverController.getInstance = function () {
      if (!this.instance_) {
          this.instance_ = new ResizeObserverController();
      }

      return this.instance_;
  };

  ResizeObserverController.instance_ = null;

  /**
   * Defines non-writable/enumerable properties of the provided target object.
   *
   * @param {Object} target - Object for which to define properties.
   * @param {Object} props - Properties to be defined.
   * @returns {Object} Target object.
   */
  var defineConfigurable = (function (target, props) {
      for (var i = 0, list = Object.keys(props); i < list.length; i += 1) {
          var key = list[i];

          Object.defineProperty(target, key, {
              value: props[key],
              enumerable: false,
              writable: false,
              configurable: true
          });
      }

      return target;
  });

  /**
   * Returns the global object associated with provided element.
   *
   * @param {Object} target
   * @returns {Object}
   */
  var getWindowOf = (function (target) {
      // Assume that the element is an instance of Node, which means that it
      // has the "ownerDocument" property from which we can retrieve a
      // corresponding global object.
      var ownerGlobal = target && target.ownerDocument && target.ownerDocument.defaultView;

      // Return the local global object if it's not possible extract one from
      // provided element.
      return ownerGlobal || global$1;
  });

  // Placeholder of an empty content rectangle.
  var emptyRect = createRectInit(0, 0, 0, 0);

  /**
   * Converts provided string to a number.
   *
   * @param {number|string} value
   * @returns {number}
   */
  function toFloat(value) {
      return parseFloat(value) || 0;
  }

  /**
   * Extracts borders size from provided styles.
   *
   * @param {CSSStyleDeclaration} styles
   * @param {...string} positions - Borders positions (top, right, ...)
   * @returns {number}
   */
  function getBordersSize(styles) {
      var positions = [], len = arguments.length - 1;
      while ( len-- > 0 ) positions[ len ] = arguments[ len + 1 ];

      return positions.reduce(function (size, position) {
          var value = styles['border-' + position + '-width'];

          return size + toFloat(value);
      }, 0);
  }

  /**
   * Extracts paddings sizes from provided styles.
   *
   * @param {CSSStyleDeclaration} styles
   * @returns {Object} Paddings box.
   */
  function getPaddings(styles) {
      var positions = ['top', 'right', 'bottom', 'left'];
      var paddings = {};

      for (var i = 0, list = positions; i < list.length; i += 1) {
          var position = list[i];

          var value = styles['padding-' + position];

          paddings[position] = toFloat(value);
      }

      return paddings;
  }

  /**
   * Calculates content rectangle of provided SVG element.
   *
   * @param {SVGGraphicsElement} target - Element content rectangle of which needs
   *      to be calculated.
   * @returns {DOMRectInit}
   */
  function getSVGContentRect(target) {
      var bbox = target.getBBox();

      return createRectInit(0, 0, bbox.width, bbox.height);
  }

  /**
   * Calculates content rectangle of provided HTMLElement.
   *
   * @param {HTMLElement} target - Element for which to calculate the content rectangle.
   * @returns {DOMRectInit}
   */
  function getHTMLElementContentRect(target) {
      // Client width & height properties can't be
      // used exclusively as they provide rounded values.
      var clientWidth = target.clientWidth;
      var clientHeight = target.clientHeight;

      // By this condition we can catch all non-replaced inline, hidden and
      // detached elements. Though elements with width & height properties less
      // than 0.5 will be discarded as well.
      //
      // Without it we would need to implement separate methods for each of
      // those cases and it's not possible to perform a precise and performance
      // effective test for hidden elements. E.g. even jQuery's ':visible' filter
      // gives wrong results for elements with width & height less than 0.5.
      if (!clientWidth && !clientHeight) {
          return emptyRect;
      }

      var styles = getWindowOf(target).getComputedStyle(target);
      var paddings = getPaddings(styles);
      var horizPad = paddings.left + paddings.right;
      var vertPad = paddings.top + paddings.bottom;

      // Computed styles of width & height are being used because they are the
      // only dimensions available to JS that contain non-rounded values. It could
      // be possible to utilize the getBoundingClientRect if only it's data wasn't
      // affected by CSS transformations let alone paddings, borders and scroll bars.
      var width = toFloat(styles.width),
          height = toFloat(styles.height);

      // Width & height include paddings and borders when the 'border-box' box
      // model is applied (except for IE).
      if (styles.boxSizing === 'border-box') {
          // Following conditions are required to handle Internet Explorer which
          // doesn't include paddings and borders to computed CSS dimensions.
          //
          // We can say that if CSS dimensions + paddings are equal to the "client"
          // properties then it's either IE, and thus we don't need to subtract
          // anything, or an element merely doesn't have paddings/borders styles.
          if (Math.round(width + horizPad) !== clientWidth) {
              width -= getBordersSize(styles, 'left', 'right') + horizPad;
          }

          if (Math.round(height + vertPad) !== clientHeight) {
              height -= getBordersSize(styles, 'top', 'bottom') + vertPad;
          }
      }

      // Following steps can't be applied to the document's root element as its
      // client[Width/Height] properties represent viewport area of the window.
      // Besides, it's as well not necessary as the <html> itself neither has
      // rendered scroll bars nor it can be clipped.
      if (!isDocumentElement(target)) {
          // In some browsers (only in Firefox, actually) CSS width & height
          // include scroll bars size which can be removed at this step as scroll
          // bars are the only difference between rounded dimensions + paddings
          // and "client" properties, though that is not always true in Chrome.
          var vertScrollbar = Math.round(width + horizPad) - clientWidth;
          var horizScrollbar = Math.round(height + vertPad) - clientHeight;

          // Chrome has a rather weird rounding of "client" properties.
          // E.g. for an element with content width of 314.2px it sometimes gives
          // the client width of 315px and for the width of 314.7px it may give
          // 314px. And it doesn't happen all the time. So just ignore this delta
          // as a non-relevant.
          if (Math.abs(vertScrollbar) !== 1) {
              width -= vertScrollbar;
          }

          if (Math.abs(horizScrollbar) !== 1) {
              height -= horizScrollbar;
          }
      }

      return createRectInit(paddings.left, paddings.top, width, height);
  }

  /**
   * Checks whether provided element is an instance of the SVGGraphicsElement.
   *
   * @param {Element} target - Element to be checked.
   * @returns {boolean}
   */
  var isSVGGraphicsElement = (function () {
      // Some browsers, namely IE and Edge, don't have the SVGGraphicsElement
      // interface.
      if (typeof SVGGraphicsElement !== 'undefined') {
          return function (target) { return target instanceof getWindowOf(target).SVGGraphicsElement; };
      }

      // If it's so, then check that element is at least an instance of the
      // SVGElement and that it has the "getBBox" method.
      // eslint-disable-next-line no-extra-parens
      return function (target) { return target instanceof getWindowOf(target).SVGElement && typeof target.getBBox === 'function'; };
  })();

  /**
   * Checks whether provided element is a document element (<html>).
   *
   * @param {Element} target - Element to be checked.
   * @returns {boolean}
   */
  function isDocumentElement(target) {
      return target === getWindowOf(target).document.documentElement;
  }

  /**
   * Calculates an appropriate content rectangle for provided html or svg element.
   *
   * @param {Element} target - Element content rectangle of which needs to be calculated.
   * @returns {DOMRectInit}
   */
  function getContentRect(target) {
      if (!isBrowser) {
          return emptyRect;
      }

      if (isSVGGraphicsElement(target)) {
          return getSVGContentRect(target);
      }

      return getHTMLElementContentRect(target);
  }

  /**
   * Creates rectangle with an interface of the DOMRectReadOnly.
   * Spec: https://drafts.fxtf.org/geometry/#domrectreadonly
   *
   * @param {DOMRectInit} rectInit - Object with rectangle's x/y coordinates and dimensions.
   * @returns {DOMRectReadOnly}
   */
  function createReadOnlyRect(ref) {
      var x = ref.x;
      var y = ref.y;
      var width = ref.width;
      var height = ref.height;

      // If DOMRectReadOnly is available use it as a prototype for the rectangle.
      var Constr = typeof DOMRectReadOnly !== 'undefined' ? DOMRectReadOnly : Object;
      var rect = Object.create(Constr.prototype);

      // Rectangle's properties are not writable and non-enumerable.
      defineConfigurable(rect, {
          x: x, y: y, width: width, height: height,
          top: y,
          right: x + width,
          bottom: height + y,
          left: x
      });

      return rect;
  }

  /**
   * Creates DOMRectInit object based on the provided dimensions and the x/y coordinates.
   * Spec: https://drafts.fxtf.org/geometry/#dictdef-domrectinit
   *
   * @param {number} x - X coordinate.
   * @param {number} y - Y coordinate.
   * @param {number} width - Rectangle's width.
   * @param {number} height - Rectangle's height.
   * @returns {DOMRectInit}
   */
  function createRectInit(x, y, width, height) {
      return { x: x, y: y, width: width, height: height };
  }

  /**
   * Class that is responsible for computations of the content rectangle of
   * provided DOM element and for keeping track of it's changes.
   */
  var ResizeObservation = function(target) {
      this.broadcastWidth = 0;
      this.broadcastHeight = 0;
      this.contentRect_ = createRectInit(0, 0, 0, 0);

      this.target = target;
  };

  /**
   * Updates content rectangle and tells whether it's width or height properties
   * have changed since the last broadcast.
   *
   * @returns {boolean}
   */


  /**
   * Reference to the last observed content rectangle.
   *
   * @private {DOMRectInit}
   */


  /**
   * Broadcasted width of content rectangle.
   *
   * @type {number}
   */
  ResizeObservation.prototype.isActive = function () {
      var rect = getContentRect(this.target);

      this.contentRect_ = rect;

      return rect.width !== this.broadcastWidth || rect.height !== this.broadcastHeight;
  };

  /**
   * Updates 'broadcastWidth' and 'broadcastHeight' properties with a data
   * from the corresponding properties of the last observed content rectangle.
   *
   * @returns {DOMRectInit} Last observed content rectangle.
   */
  ResizeObservation.prototype.broadcastRect = function () {
      var rect = this.contentRect_;

      this.broadcastWidth = rect.width;
      this.broadcastHeight = rect.height;

      return rect;
  };

  var ResizeObserverEntry = function(target, rectInit) {
      var contentRect = createReadOnlyRect(rectInit);

      // According to the specification following properties are not writable
      // and are also not enumerable in the native implementation.
      //
      // Property accessors are not being used as they'd require to define a
      // private WeakMap storage which may cause memory leaks in browsers that
      // don't support this type of collections.
      defineConfigurable(this, { target: target, contentRect: contentRect });
  };

  var ResizeObserverSPI = function(callback, controller, callbackCtx) {
      this.activeObservations_ = [];
      this.observations_ = new MapShim();

      if (typeof callback !== 'function') {
          throw new TypeError('The callback provided as parameter 1 is not a function.');
      }

      this.callback_ = callback;
      this.controller_ = controller;
      this.callbackCtx_ = callbackCtx;
  };

  /**
   * Starts observing provided element.
   *
   * @param {Element} target - Element to be observed.
   * @returns {void}
   */


  /**
   * Registry of the ResizeObservation instances.
   *
   * @private {Map<Element, ResizeObservation>}
   */


  /**
   * Public ResizeObserver instance which will be passed to the callback
   * function and used as a value of it's "this" binding.
   *
   * @private {ResizeObserver}
   */

  /**
   * Collection of resize observations that have detected changes in dimensions
   * of elements.
   *
   * @private {Array<ResizeObservation>}
   */
  ResizeObserverSPI.prototype.observe = function (target) {
      if (!arguments.length) {
          throw new TypeError('1 argument required, but only 0 present.');
      }

      // Do nothing if current environment doesn't have the Element interface.
      if (typeof Element === 'undefined' || !(Element instanceof Object)) {
          return;
      }

      if (!(target instanceof getWindowOf(target).Element)) {
          throw new TypeError('parameter 1 is not of type "Element".');
      }

      var observations = this.observations_;

      // Do nothing if element is already being observed.
      if (observations.has(target)) {
          return;
      }

      observations.set(target, new ResizeObservation(target));

      this.controller_.addObserver(this);

      // Force the update of observations.
      this.controller_.refresh();
  };

  /**
   * Stops observing provided element.
   *
   * @param {Element} target - Element to stop observing.
   * @returns {void}
   */
  ResizeObserverSPI.prototype.unobserve = function (target) {
      if (!arguments.length) {
          throw new TypeError('1 argument required, but only 0 present.');
      }

      // Do nothing if current environment doesn't have the Element interface.
      if (typeof Element === 'undefined' || !(Element instanceof Object)) {
          return;
      }

      if (!(target instanceof getWindowOf(target).Element)) {
          throw new TypeError('parameter 1 is not of type "Element".');
      }

      var observations = this.observations_;

      // Do nothing if element is not being observed.
      if (!observations.has(target)) {
          return;
      }

      observations.delete(target);

      if (!observations.size) {
          this.controller_.removeObserver(this);
      }
  };

  /**
   * Stops observing all elements.
   *
   * @returns {void}
   */
  ResizeObserverSPI.prototype.disconnect = function () {
      this.clearActive();
      this.observations_.clear();
      this.controller_.removeObserver(this);
  };

  /**
   * Collects observation instances the associated element of which has changed
   * it's content rectangle.
   *
   * @returns {void}
   */
  ResizeObserverSPI.prototype.gatherActive = function () {
          var this$1 = this;

      this.clearActive();

      this.observations_.forEach(function (observation) {
          if (observation.isActive()) {
              this$1.activeObservations_.push(observation);
          }
      });
  };

  /**
   * Invokes initial callback function with a list of ResizeObserverEntry
   * instances collected from active resize observations.
   *
   * @returns {void}
   */
  ResizeObserverSPI.prototype.broadcastActive = function () {
      // Do nothing if observer doesn't have active observations.
      if (!this.hasActive()) {
          return;
      }

      var ctx = this.callbackCtx_;

      // Create ResizeObserverEntry instance for every active observation.
      var entries = this.activeObservations_.map(function (observation) {
          return new ResizeObserverEntry(observation.target, observation.broadcastRect());
      });

      this.callback_.call(ctx, entries, ctx);
      this.clearActive();
  };

  /**
   * Clears the collection of active observations.
   *
   * @returns {void}
   */
  ResizeObserverSPI.prototype.clearActive = function () {
      this.activeObservations_.splice(0);
  };

  /**
   * Tells whether observer has active observations.
   *
   * @returns {boolean}
   */
  ResizeObserverSPI.prototype.hasActive = function () {
      return this.activeObservations_.length > 0;
  };

  // Registry of internal observers. If WeakMap is not available use current shim
  // for the Map collection as it has all required methods and because WeakMap
  // can't be fully polyfilled anyway.
  var observers = typeof WeakMap !== 'undefined' ? new WeakMap() : new MapShim();

  /**
   * ResizeObserver API. Encapsulates the ResizeObserver SPI implementation
   * exposing only those methods and properties that are defined in the spec.
   */
  var ResizeObserver = function(callback) {
      if (!(this instanceof ResizeObserver)) {
          throw new TypeError('Cannot call a class as a function.');
      }
      if (!arguments.length) {
          throw new TypeError('1 argument required, but only 0 present.');
      }

      var controller = ResizeObserverController.getInstance();
      var observer = new ResizeObserverSPI(callback, controller, this);

      observers.set(this, observer);
  };

  // Expose public methods of ResizeObserver.
  ['observe', 'unobserve', 'disconnect'].forEach(function (method) {
      ResizeObserver.prototype[method] = function () {
          return (ref = observers.get(this))[method].apply(ref, arguments);
          var ref;
      };
  });

  var index = (function () {
      // Export existing implementation if available.
      if (typeof global$1.ResizeObserver !== 'undefined') {
          return global$1.ResizeObserver;
      }

      return ResizeObserver;
  })();

  var canUseDOM = !!(
    typeof window !== 'undefined' &&
    window.document &&
    window.document.createElement
  );

  var canUseDom = canUseDOM;

  var SimpleBar =
  /*#__PURE__*/
  function () {
    function SimpleBar(element, options) {
      var _this = this;

      _classCallCheck(this, SimpleBar);

      this.onScrollX = function () {
        if (!_this.scrollXTicking) {
          window.requestAnimationFrame(_this.scrollX);
          _this.scrollXTicking = true;
        }
      };

      this.onScrollY = function () {
        if (!_this.scrollYTicking) {
          window.requestAnimationFrame(_this.scrollY);
          _this.scrollYTicking = true;
        }
      };

      this.scrollX = function () {
        _this.showScrollbar('x');

        _this.positionScrollbar('x');

        _this.scrollXTicking = false;
      };

      this.scrollY = function () {
        _this.showScrollbar('y');

        _this.positionScrollbar('y');

        _this.scrollYTicking = false;
      };

      this.onMouseEnter = function () {
        _this.showScrollbar('x');

        _this.showScrollbar('y');
      };

      this.onMouseMove = function (e) {
        var bboxY = _this.trackY.getBoundingClientRect();

        var bboxX = _this.trackX.getBoundingClientRect();

        _this.mouseX = e.clientX;
        _this.mouseY = e.clientY;

        if (_this.isWithinBounds(bboxY)) {
          _this.showScrollbar('y');
        }

        if (_this.isWithinBounds(bboxX)) {
          _this.showScrollbar('x');
        }
      };

      this.onWindowResize = function () {
        _this.hideNativeScrollbar();
      };

      this.hideScrollbars = function () {
        var bboxY = _this.trackY.getBoundingClientRect();

        var bboxX = _this.trackX.getBoundingClientRect();

        if (!_this.isWithinBounds(bboxY)) {
          _this.scrollbarY.classList.remove('visible');

          _this.isVisible.y = false;
        }

        if (!_this.isWithinBounds(bboxX)) {
          _this.scrollbarX.classList.remove('visible');

          _this.isVisible.x = false;
        }
      };

      this.onMouseDown = function (e) {
        var bboxY = _this.scrollbarY.getBoundingClientRect();

        var bboxX = _this.scrollbarX.getBoundingClientRect();

        if (_this.isWithinBounds(bboxY)) {
          e.preventDefault();

          _this.onDrag(e, 'y');
        }

        if (_this.isWithinBounds(bboxX)) {
          e.preventDefault();

          _this.onDrag(e, 'x');
        }
      };

      this.drag = function (e) {
        var eventOffset, track, scrollEl;
        e.preventDefault();

        if (_this.currentAxis === 'y') {
          eventOffset = e.pageY;
          track = _this.trackY;
          scrollEl = _this.scrollContentEl;
        } else {
          eventOffset = e.pageX;
          track = _this.trackX;
          scrollEl = _this.contentEl;
        } // Calculate how far the user's mouse is from the top/left of the scrollbar (minus the dragOffset).


        var dragPos = eventOffset - track.getBoundingClientRect()[_this.offsetAttr[_this.currentAxis]] - _this.dragOffset[_this.currentAxis]; // Convert the mouse position into a percentage of the scrollbar height/width.


        var dragPerc = dragPos / track[_this.sizeAttr[_this.currentAxis]]; // Scroll the content by the same percentage.

        var scrollPos = dragPerc * _this.contentEl[_this.scrollSizeAttr[_this.currentAxis]];
        scrollEl[_this.scrollOffsetAttr[_this.currentAxis]] = scrollPos;
      };

      this.onEndDrag = function () {
        document.removeEventListener('mousemove', _this.drag);
        document.removeEventListener('mouseup', _this.onEndDrag);
      };

      this.el = element;
      this.flashTimeout;
      this.contentEl;
      this.scrollContentEl;
      this.dragOffset = {
        x: 0,
        y: 0
      };
      this.isEnabled = {
        x: true,
        y: true
      };
      this.isVisible = {
        x: false,
        y: false
      };
      this.scrollOffsetAttr = {
        x: 'scrollLeft',
        y: 'scrollTop'
      };
      this.sizeAttr = {
        x: 'offsetWidth',
        y: 'offsetHeight'
      };
      this.scrollSizeAttr = {
        x: 'scrollWidth',
        y: 'scrollHeight'
      };
      this.offsetAttr = {
        x: 'left',
        y: 'top'
      };
      this.handleSize = {
        x: 0,
        y: 0
      };
      this.globalObserver;
      this.mutationObserver;
      this.resizeObserver;
      this.currentAxis;
      this.scrollbarWidth;
      this.options = Object.assign({}, SimpleBar.defaultOptions, options);
      this.isRtl = this.options.direction === 'rtl';
      this.classNames = this.options.classNames;
      this.offsetSize = 20;
      this.recalculateImmediate = this.recalculate.bind(this);
      this.recalculate = lodash_throttle(this.recalculate.bind(this), 1000);
      this.onMouseMove = lodash_throttle(this.onMouseMove.bind(this), 100);
      this.init();
    }

    _createClass(SimpleBar, [{
      key: "init",
      value: function init() {
        // Save a reference to the instance, so we know this DOM node has already been instancied
        this.el.SimpleBar = this;
        this.initDOM(); // We stop here on server-side

        if (canUseDom) {
          // Calculate content size
          this.hideNativeScrollbar();
          this.render();
          this.initListeners();
        }
      }
    }, {
      key: "initDOM",
      value: function initDOM() {
        var _this2 = this;

        // make sure this element doesn't have the elements yet
        if (Array.from(this.el.children).filter(function (child) {
          return child.classList.contains(_this2.classNames.scrollContent);
        }).length) {
          // assume that element has his DOM already initiated
          this.trackX = this.el.querySelector(".".concat(this.classNames.track, ".horizontal"));
          this.trackY = this.el.querySelector(".".concat(this.classNames.track, ".vertical"));
          this.scrollContentEl = this.el.querySelector(".".concat(this.classNames.scrollContent));
          this.contentEl = this.el.querySelector(".".concat(this.classNames.content));
        } else {
          // Prepare DOM
          this.scrollContentEl = document.createElement('div');
          this.contentEl = document.createElement('div');
          this.scrollContentEl.classList.add(this.classNames.scrollContent);
          this.contentEl.classList.add(this.classNames.content);

          while (this.el.firstChild) {
            this.contentEl.appendChild(this.el.firstChild);
          }

          this.scrollContentEl.appendChild(this.contentEl);
          this.el.appendChild(this.scrollContentEl);
        }

        if (!this.trackX || !this.trackY) {
          var track = document.createElement('div');
          var scrollbar = document.createElement('div');
          track.classList.add(this.classNames.track);
          scrollbar.classList.add(this.classNames.scrollbar);

          if (!this.options.autoHide) {
            scrollbar.classList.add('visible');
          }

          track.appendChild(scrollbar);
          this.trackX = track.cloneNode(true);
          this.trackX.classList.add('horizontal');
          this.trackY = track.cloneNode(true);
          this.trackY.classList.add('vertical');
          this.el.insertBefore(this.trackX, this.el.firstChild);
          this.el.insertBefore(this.trackY, this.el.firstChild);
        }

        this.scrollbarX = this.trackX.querySelector(".".concat(this.classNames.scrollbar));
        this.scrollbarY = this.trackY.querySelector(".".concat(this.classNames.scrollbar));
        this.el.setAttribute('data-simplebar', 'init');
      }
    }, {
      key: "initListeners",
      value: function initListeners() {
        var _this3 = this;

        // Event listeners
        if (this.options.autoHide) {
          this.el.addEventListener('mouseenter', this.onMouseEnter);
        }

        this.el.addEventListener('mousedown', this.onMouseDown);
        this.el.addEventListener('mousemove', this.onMouseMove);
        this.contentEl.addEventListener('scroll', this.onScrollX);
        this.scrollContentEl.addEventListener('scroll', this.onScrollY); // Browser zoom triggers a window resize

        window.addEventListener('resize', this.onWindowResize); // MutationObserver is IE11+

        if (typeof MutationObserver !== 'undefined') {
          // create an observer instance
          this.mutationObserver = new MutationObserver(function (mutations) {
            mutations.forEach(function (mutation) {
              if (_this3.isChildNode(mutation.target) || mutation.addedNodes.length) {
                _this3.recalculate();
              }
            });
          }); // pass in the target node, as well as the observer options

          this.mutationObserver.observe(this.el, {
            attributes: true,
            childList: true,
            characterData: true,
            subtree: true
          });
        }

        this.resizeObserver = new index(this.recalculate);
        this.resizeObserver.observe(this.el);
      }
      /**
       * Recalculate scrollbar
       */

    }, {
      key: "recalculate",
      value: function recalculate() {
        this.render();
      }
    }, {
      key: "render",
      value: function render() {
        this.contentSizeX = this.contentEl[this.scrollSizeAttr['x']];
        this.contentSizeY = this.contentEl[this.scrollSizeAttr['y']] - (this.scrollbarWidth || this.offsetSize);
        this.trackXSize = this.trackX[this.sizeAttr['x']];
        this.trackYSize = this.trackY[this.sizeAttr['y']]; // Set isEnabled to false if scrollbar is not necessary (content is shorter than wrapper)

        this.isEnabled['x'] = this.trackXSize < this.contentSizeX;
        this.isEnabled['y'] = this.trackYSize < this.contentSizeY;
        this.resizeScrollbar('x');
        this.resizeScrollbar('y');
        this.positionScrollbar('x');
        this.positionScrollbar('y');
        this.toggleTrackVisibility('x');
        this.toggleTrackVisibility('y');
      }
      /**
       * Resize scrollbar
       */

    }, {
      key: "resizeScrollbar",
      value: function resizeScrollbar() {
        var axis = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 'y';
        var scrollbar;
        var contentSize;
        var trackSize;

        if (!this.isEnabled[axis] && !this.options.forceVisible) {
          return;
        }

        if (axis === 'x') {
          scrollbar = this.scrollbarX;
          contentSize = this.contentSizeX;
          trackSize = this.trackXSize;
        } else {
          // 'y'
          scrollbar = this.scrollbarY;
          contentSize = this.contentSizeY;
          trackSize = this.trackYSize;
        }

        var scrollbarRatio = trackSize / contentSize; // Calculate new height/position of drag handle.

        this.handleSize[axis] = Math.max(~~(scrollbarRatio * trackSize), this.options.scrollbarMinSize);

        if (this.options.scrollbarMaxSize) {
          this.handleSize[axis] = Math.min(this.handleSize[axis], this.options.scrollbarMaxSize);
        }

        if (axis === 'x') {
          scrollbar.style.width = "".concat(this.handleSize[axis], "px");
        } else {
          scrollbar.style.height = "".concat(this.handleSize[axis], "px");
        }
      }
    }, {
      key: "positionScrollbar",
      value: function positionScrollbar() {
        var axis = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 'y';
        var scrollbar;
        var scrollOffset;
        var contentSize;
        var trackSize;

        if (axis === 'x') {
          scrollbar = this.scrollbarX;
          scrollOffset = this.contentEl[this.scrollOffsetAttr[axis]]; // Either scrollTop() or scrollLeft().

          contentSize = this.contentSizeX;
          trackSize = this.trackXSize;
        } else {
          // 'y'
          scrollbar = this.scrollbarY;
          scrollOffset = this.scrollContentEl[this.scrollOffsetAttr[axis]]; // Either scrollTop() or scrollLeft().

          contentSize = this.contentSizeY;
          trackSize = this.trackYSize;
        }

        var scrollPourcent = scrollOffset / (contentSize - trackSize);
        var handleOffset = ~~((trackSize - this.handleSize[axis]) * scrollPourcent);

        if (this.isEnabled[axis] || this.options.forceVisible) {
          if (axis === 'x') {
            scrollbar.style.transform = "translate3d(".concat(handleOffset, "px, 0, 0)");
          } else {
            scrollbar.style.transform = "translate3d(0, ".concat(handleOffset, "px, 0)");
          }
        }
      }
    }, {
      key: "toggleTrackVisibility",
      value: function toggleTrackVisibility() {
        var axis = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 'y';
        var track = axis === 'y' ? this.trackY : this.trackX;
        var scrollbar = axis === 'y' ? this.scrollbarY : this.scrollbarX;

        if (this.isEnabled[axis] || this.options.forceVisible) {
          track.style.visibility = 'visible';
        } else {
          track.style.visibility = 'hidden';
        } // Even if forceVisible is enabled, scrollbar itself should be hidden


        if (this.options.forceVisible) {
          if (this.isEnabled[axis]) {
            scrollbar.style.visibility = 'visible';
          } else {
            scrollbar.style.visibility = 'hidden';
          }
        }
      }
    }, {
      key: "hideNativeScrollbar",
      value: function hideNativeScrollbar() {
        // Recalculate scrollbarWidth in case it's a zoom
        var offset = 20; // matched in style.css: [data-simplebar="init"] {...}
        this.scrollbarWidth = scrollbarWidth();
        this.scrollContentEl.style[this.isRtl ? 'paddingLeft' : 'paddingRight'] = "".concat((this.scrollbarWidth || this.offsetSize)+offset, "px");
        this.scrollContentEl.style.marginBottom = "-".concat(this.scrollbarWidth * 2 || this.offsetSize, "px");
        this.contentEl.style.paddingBottom = "".concat(this.scrollbarWidth || this.offsetSize, "px");

        if (this.scrollbarWidth !== 0) {
          this.contentEl.style[this.isRtl ? 'marginLeft' : 'marginRight'] = "-".concat(offset, "px");
        }
      }
      /**
       * On scroll event handling
       */

    }, {
      key: "showScrollbar",

      /**
       * Show scrollbar
       */
      value: function showScrollbar() {
        var axis = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 'y';
        var scrollbar; // Scrollbar already visible

        if (this.isVisible[axis]) {
          return;
        }

        if (axis === 'x') {
          scrollbar = this.scrollbarX;
        } else {
          // 'y'
          scrollbar = this.scrollbarY;
        }

        if (this.isEnabled[axis]) {
          scrollbar.classList.add('visible');
          this.isVisible[axis] = true;
        }

        if (!this.options.autoHide) {
          return;
        }

        window.clearInterval(this.flashTimeout);
        this.flashTimeout = window.setInterval(this.hideScrollbars, this.options.timeout);
      }
      /**
       * Hide Scrollbar
       */

    }, {
      key: "onDrag",

      /**
       * on scrollbar handle drag
       */
      value: function onDrag(e) {
        var axis = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 'y';
        // Preventing the event's default action stops text being
        // selectable during the drag.
        e.preventDefault();
        var scrollbar = axis === 'y' ? this.scrollbarY : this.scrollbarX; // Measure how far the user's mouse is from the top of the scrollbar drag handle.

        var eventOffset = axis === 'y' ? e.pageY : e.pageX;
        this.dragOffset[axis] = eventOffset - scrollbar.getBoundingClientRect()[this.offsetAttr[axis]];
        this.currentAxis = axis;
        document.addEventListener('mousemove', this.drag);
        document.addEventListener('mouseup', this.onEndDrag);
      }
      /**
       * Drag scrollbar handle
       */

    }, {
      key: "getScrollElement",

      /**
       * Getter for original scrolling element
       */
      value: function getScrollElement() {
        var axis = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 'y';
        return axis === 'y' ? this.scrollContentEl : this.contentEl;
      }
      /**
       * Getter for content element
       */

    }, {
      key: "getContentElement",
      value: function getContentElement() {
        return this.contentEl;
      }
    }, {
      key: "removeListeners",
      value: function removeListeners() {
        // Event listeners
        if (this.options.autoHide) {
          this.el.removeEventListener('mouseenter', this.onMouseEnter);
        }

        this.scrollContentEl.removeEventListener('scroll', this.onScrollY);
        this.contentEl.removeEventListener('scroll', this.onScrollX);
        this.mutationObserver.disconnect();
        this.resizeObserver.disconnect();
      }
      /**
       * UnMount mutation observer and delete SimpleBar instance from DOM element
       */

    }, {
      key: "unMount",
      value: function unMount() {
        this.removeListeners();
        this.el.SimpleBar = null;
      }
      /**
       * Recursively walks up the parent nodes looking for this.el
       */

    }, {
      key: "isChildNode",
      value: function isChildNode(el) {
        if (el === null) return false;
        if (el === this.el) return true;
        return this.isChildNode(el.parentNode);
      }
      /**
       * Check if mouse is within bounds
       */

    }, {
      key: "isWithinBounds",
      value: function isWithinBounds(bbox) {
        return this.mouseX >= bbox.left && this.mouseX <= bbox.left + bbox.width && this.mouseY >= bbox.top && this.mouseY <= bbox.top + bbox.height;
      }
    }], [{
      key: "initHtmlApi",
      value: function initHtmlApi() {
        this.initDOMLoadedElements = this.initDOMLoadedElements.bind(this); // MutationObserver is IE11+

        if (typeof MutationObserver !== 'undefined') {
          // Mutation observer to observe dynamically added elements
          this.globalObserver = new MutationObserver(function (mutations) {
            mutations.forEach(function (mutation) {
              Array.from(mutation.addedNodes).forEach(function (addedNode) {
                if (addedNode.nodeType === 1) {
                  if (addedNode.hasAttribute('data-simplebar')) {
                    !addedNode.SimpleBar && new SimpleBar(addedNode, SimpleBar.getElOptions(addedNode));
                  } else {
                    Array.from(addedNode.querySelectorAll('[data-simplebar]')).forEach(function (el) {
                      !el.SimpleBar && new SimpleBar(el, SimpleBar.getElOptions(el));
                    });
                  }
                }
              });
              Array.from(mutation.removedNodes).forEach(function (removedNode) {
                if (removedNode.nodeType === 1) {
                  if (removedNode.hasAttribute('data-simplebar')) {
                    removedNode.SimpleBar && removedNode.SimpleBar.unMount();
                  } else {
                    Array.from(removedNode.querySelectorAll('[data-simplebar]')).forEach(function (el) {
                      el.SimpleBar && el.SimpleBar.unMount();
                    });
                  }
                }
              });
            });
          });
          this.globalObserver.observe(document, {
            childList: true,
            subtree: true
          });
        } // Taken from jQuery `ready` function
        // Instantiate elements already present on the page


        if (document.readyState === 'complete' || document.readyState !== 'loading' && !document.documentElement.doScroll) {
          // Handle it asynchronously to allow scripts the opportunity to delay init
          window.setTimeout(this.initDOMLoadedElements);
        } else {
          document.addEventListener('DOMContentLoaded', this.initDOMLoadedElements);
          window.addEventListener('load', this.initDOMLoadedElements);
        }
      } // Helper function to retrieve options from element attributes

    }, {
      key: "getElOptions",
      value: function getElOptions(el) {
        var options = Array.from(el.attributes).reduce(function (acc, attribute) {
          var option = attribute.name.match(/data-simplebar-(.+)/);

          if (option) {
            var key = option[1].replace(/\W+(.)/g, function (x, chr) {
              return chr.toUpperCase();
            });

            switch (attribute.value) {
              case 'true':
                acc[key] = true;
                break;

              case 'false':
                acc[key] = false;
                break;

              case undefined:
                acc[key] = true;
                break;

              default:
                acc[key] = attribute.value;
            }
          }

          return acc;
        }, {});
        return options;
      }
    }, {
      key: "removeObserver",
      value: function removeObserver() {
        this.globalObserver.disconnect();
      }
    }, {
      key: "initDOMLoadedElements",
      value: function initDOMLoadedElements() {
        document.removeEventListener('DOMContentLoaded', this.initDOMLoadedElements);
        window.removeEventListener('load', this.initDOMLoadedElements);
        Array.from(document.querySelectorAll('[data-simplebar]')).forEach(function (el) {
          if (!el.SimpleBar) new SimpleBar(el, SimpleBar.getElOptions(el));
        });
      }
    }, {
      key: "defaultOptions",
      get: function get() {
        return {
          autoHide: true,
          forceVisible: false,
          classNames: {
            content: 'simplebar-content',
            scrollContent: 'simplebar-scroll-content',
            scrollbar: 'simplebar-scrollbar',
            track: 'simplebar-track'
          },
          scrollbarMinSize: 25,
          scrollbarMaxSize: 0,
          direction: 'ltr',
          timeout: 1000
        };
      }
    }]);

    return SimpleBar;
  }();

  if (canUseDom) {
    SimpleBar.initHtmlApi();
  }

  return SimpleBar;

})));
