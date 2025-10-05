import sys
import os

def extract_pdf(filepath):
    try:
        from pypdf import PdfReader
        reader = PdfReader(filepath)
        text = ""
        for page in reader.pages:
            text += page.extract_text() + "\n\n"
        return text
    except ImportError:
        return "Error: pypdf library not installed. Run: pip install pypdf"
    except Exception as e:
        return f"Error extracting PDF: {e}"

def extract_txt(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return f.read()
    except UnicodeDecodeError:
        try:
            with open(filepath, 'r', encoding='latin-1') as f:
                return f.read()
        except Exception as e:
            return f"Error reading TXT file: {e}"
    except Exception as e:
        return f"Error reading TXT file: {e}"

def extract_csv(filepath, delimiter=','):
    try:
        import csv
        text = ""
        with open(filepath, 'r', encoding='utf-8') as f:
            reader = csv.reader(f, delimiter=delimiter)
            for row in reader:
                text += " | ".join([str(cell) if cell is not None else "" for cell in row]) + "\n"
        return text
    except Exception as e:
        return f"Error reading {delimiter.upper()} file: {e}"

def extract_docx(filepath):
    try:
        from docx import Document
        doc = Document(filepath)
        text = ""
        for para in doc.paragraphs:
            text += para.text + "\n"
        return text
    except ImportError:
        return "Error: python-docx library not installed. Run: pip install python-docx"
    except Exception as e:
        return f"Error extracting DOCX: {e}"

def extract_text(filepath, expected_type=None):
    _, ext = os.path.splitext(filepath)
    ext = ext.lower().lstrip('.')

    if expected_type and ext != expected_type.lower():
        return f"ERROR_WRONG_TYPE: Expected {expected_type.upper()} file, but got {ext.upper()} file"

    extractors = {
        'pdf': extract_pdf,
        'txt': extract_txt,
        'csv': extract_csv,
        'tsv': lambda f: extract_csv(f, delimiter='\t'),
        'docx': extract_docx,
        'doc': lambda f: "Error: DOC files not supported. Please convert to DOCX"
    }

    if ext in extractors:
        return extractors[ext](filepath)
    else:
        return f"Error: Unsupported file type '.{ext}'"

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python extract_text.py <filepath> [expected_type]")
        sys.exit(1)
    
    filepath = sys.argv[1]
    expected_type = sys.argv[2] if len(sys.argv) > 2 else None
    
    text = extract_text(filepath, expected_type)
    
    with open("text.txt", 'w', encoding='utf-8') as output:
        output.write(text)
