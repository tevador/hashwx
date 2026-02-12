/* Javascript glue code to be used with the hashwx.wasm binary. */
/* Written in 2026 by tevador <tevador@gmail.com>. */
/* Copyright waiver: This source file is released into the public domain. */

class hashwx {
    #imports;
    #instances = [1];

    #new_instance(ctx, seed, reg, mem, is_compiled) {
        let obj = { ctx: ctx, seed: seed, reg: reg, mem: mem, is_compiled: is_compiled }
        let instances = this.#instances;
        for (let i = 0; i < instances.length; ++i) {
            if (typeof instances[i] == "undefined") {
                instances[i] = obj;
                return i;
            }
        }
        instances.push(obj);
        return instances.length - 1;
    }

    #free_instance(i) {
        delete this.#instances[i];
    }

    hashwx_alloc(type) {
        if (!this.#imports) {
            let main_module = hashwx.#create_instance();
            this.#imports = main_module.exports;
			this.#imports._initialize();
        }
        let ctx = this.#imports.hashwx_alloc(type);
        if (ctx <= 0) {
            return ctx;
        }
        let seed = this.#imports.hashwx_seed(ctx);
        let reg = this.#imports.hashwx_registers(ctx);
        let mem = this.#imports.hashwx_memory(ctx);
        let is_compiled = !!(type & 1);
        return this.#new_instance(ctx, seed, reg, mem, is_compiled);
    }

    hashwx_seed(i) {
        let obj = this.#instances[i];
        return obj.seed;
    }

    hashwx_seed_array(seed) {
        return new Uint8Array(this.#imports.memory.buffer, seed, 32);
    }

    hashwx_make(i, seed_src) {
        let obj = this.#instances[i];
		let seed_dst = this.hashwx_seed_array(obj.seed);
		seed_dst.set(seed_src);
        this.#imports.hashwx_make(obj.ctx, obj.seed);
        if (!obj.is_compiled) {
            return;
        }
        let code_ptr = this.#imports.hashwx_module(obj.ctx);
        let code_size = this.#imports.hashwx_module_size(obj.ctx);
        let wasm_data = new Uint8Array(this.#imports.memory.buffer, code_ptr, code_size);
        let wasm_module = new WebAssembly.Module(wasm_data);
        obj.side_module = new WebAssembly.Instance(wasm_module, { env: { memory: this.#imports.memory } });
    }

    hashwx_exec(i, nonce) {
        let obj = this.#instances[i];
        if (!obj.is_compiled) {
            return this.#imports.hashwx_exec(obj.ctx, nonce);
        }
        this.#imports.hashwx_exec_begin(obj.ctx, nonce);
        obj.side_module.exports.exec(obj.reg, obj.mem);
        return this.#imports.hashwx_exec_final(obj.ctx);
    }

    hashwx_free(i) {
        let obj = this.#instances[i];
        if (typeof obj == "object") {
            this.#imports.hashwx_free(obj.ctx);
            this.#free_instance(i);
        }
    }

    static #hashwx_module;

    static #fetch(url) {
        var xhr = new XMLHttpRequest;
        xhr.overrideMimeType('text/plain; charset=x-user-defined');
        xhr.open("GET", url, false);
        xhr.send(null);
        let str = xhr.response;
        var buf = new ArrayBuffer(str.length);
        var buf8 = new Uint8Array(buf);
        for (let i = 0; i < str.length; i++) {
            buf8[i] = str.charCodeAt(i);
        }
        return buf;
    }

    static #create_module(filename) {
        let wasm_data;
        if (typeof process === "object" && process.versions && typeof process.versions.node == "string") {
            //nodejs
            const fs = require("fs");
            wasm_data = fs.readFileSync(__dirname + "/" + filename);
        }
        else {
            //browser
            let script_name = self.location.href;
            if (typeof document != "undefined" && document.currentScript?.src) {
                script_name = document.currentScript.src;
            }
            let script_dir = new URL(".", script_name).href;
            wasm_data = this.#fetch(script_dir + filename);
        }
        return new WebAssembly.Module(wasm_data);
    }

    static #create_instance(filename = "hashwx.wasm") {
        if (!this.#hashwx_module) {
            try {
                this.#hashwx_module = this.#create_module(filename);
            } catch (e) {
                throw new Error(`Failed to create WASM module ${filename}: ${e}`);
            }
        }
		let imports = {};
        return new WebAssembly.Instance(this.#hashwx_module, imports);
    }
}

var HWX_LIB = new hashwx();
