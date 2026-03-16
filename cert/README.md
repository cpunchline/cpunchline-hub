# 证书生成指南

## 🔐 证书生成步骤

### 方法一:分步生成
```bash
# 1. 生成RSA私钥
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -pkeyopt rsa_keygen_pubexp:65537 -out server.key

# 2. 生成证书请求文件
openssl req -new -key server.key -out server_request.csr
```

### 方法二:一键生成(推荐)
```bash
openssl req -new -newkey rsa:2048 -nodes -keyout server.key -out server_request.csr
```

## 📝 证书信息填写示例

```
Country Name (2 letter code) [AU]: CN
State or Province Name (full name) [Some-State]: ShanXi
Locality Name (eg, city) []: XiAn
Organization Name (eg, company) [Internet Widgits Pty Ltd]: code
Organizational Unit Name (eg, section) []: unit
Common Name (e.g. server FQDN or YOUR name) []: 127.0.0.1
Email Address []: cpunchline@foxmail.com

A challenge password []: 
An optional company name []: 
```

## ✅ 自签名证书生成

```bash
# 使用私钥对证书请求进行签名(自CA)
openssl x509 -req -days 365 -in server_request.csr -signkey server.key -out server.crt
```

生成结果:
```
Certificate request self-signature ok
subject=C = CN, ST = ShanXi, L = XiAn, O = code, OU = unit, CN = 127.0.0.1, emailAddress = cpunchline@foxmail.com
```
