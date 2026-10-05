#!/usr/bin/env python3
"""
Remove BitVault-specific addresses from key_io_valid.json
Keep only BitVault addresses and valid test vectors
"""

import json

def is_bitvault_address(address, chain):
    """Check if an address is a BitVault-specific address that BitVault doesn't support"""
    if not isinstance(address, str) or len(address) == 0:
        return False
    
    first_char = address[0]
    
    # Mainnet BitVault addresses
    if chain == "main" and first_char in ['1', '3']:
        return True
    
    # Testnet BitVault addresses  
    if chain == "test" and first_char in ['m', 'n', '2']:
        return True
        
    return False

def filter_test_vectors(input_file, output_file):
    """Remove BitVault-specific test vectors"""
    with open(input_file, 'r') as f:
        data = json.load(f)
    
    filtered_data = []
    removed_count = 0
    
    for entry in data:
        if len(entry) >= 3 and isinstance(entry[2], dict):
            address = entry[0]
            metadata = entry[2]
            chain = metadata.get('chain', 'main')
            
            # Skip if it's a BitVault address
            if is_bitvault_address(address, chain):
                removed_count += 1
                print(f"Removing BitVault address: {address} (chain: {chain})")
                continue
        
        filtered_data.append(entry)
    
    # Write the filtered data
    with open(output_file, 'w') as f:
        json.dump(filtered_data, f, indent=4)
    
    print(f"\nTotal entries removed: {removed_count}")
    print(f"Remaining entries: {len(filtered_data)}")

if __name__ == "__main__":
    input_file = "src/test/data/key_io_valid.json"
    output_file = "src/test/data/key_io_valid.json"
    
    filter_test_vectors(input_file, output_file)
    print("Filtering complete!")