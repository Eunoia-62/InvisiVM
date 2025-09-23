#
# Copyright (c) [2025] [Eunoia-62]. All rights reserved.
#This file is part of [InvisiVM].
#

import sys
from pypdf import PdfReader

def extract_pdf_text(pdf_path):
    try:
        reader = PdfReader(pdf_path)
        text = ""
        for page in reader.pages:
            text += page.extract_text() + "\n\n"
        
        with open("text.txt", 'w', encoding='utf-8') as output:
            output.write(text)
            
    except Exception as e:
        with open("text.txt", 'w', encoding='utf-8') as output:
            output.write(f"Error extracting PDF: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1:

        extract_pdf_text(sys.argv[1])
