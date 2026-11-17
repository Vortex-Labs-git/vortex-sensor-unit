# Configs

nano partitions.csv

# Name,     Type, SubType,  Offset,   Size
nvs,        data, nvs,      0x9000,   0x4000
otadata,    data, ota,      0xd000,   0x2000
phy_init,   data, phy,      0xf000,   0x1000
ota_0,      app,  ota_0,    0x10000,  0x180000
ota_1,      app,  ota_1,    ,         0x180000




1. Download the Google root CA

curl -o ota_ca_cert.pem https://i.pki.goog/r4.pem

2. Verify the file is correct

openssl x509 -in ota_ca_cert.pem -noout -subject -enddate

subject=C=US, O=Google Trust Services LLC, CN=GTS Root R4
notAfter=Jun 22 00:00:42 2036 GMT

3. And confirm it actually validates your server

openssl s_client -connect www.vortexlabsofficial.com:443 \
  -servername www.vortexlabsofficial.com \
  -CAfile ota_ca_cert.pem </dev/null 2>/dev/null | grep "Verify return code"